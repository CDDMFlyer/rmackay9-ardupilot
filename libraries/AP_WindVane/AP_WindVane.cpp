/*
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <AP_WindVane/AP_WindVane.h>
#include <AP_BoardConfig/AP_BoardConfig.h>
#include <RC_Channel/RC_Channel.h>
#include <AP_AHRS/AP_AHRS.h>
#include <GCS_MAVLink/GCS.h>
#include <Filter/Filter.h>
#include <utility>
#if CONFIG_HAL_BOARD == HAL_BOARD_PX4 || CONFIG_HAL_BOARD == HAL_BOARD_VRBRAIN
#include <board_config.h>
#endif
#if CONFIG_HAL_BOARD == HAL_BOARD_SITL
#include <SITL/SITL.h>
#endif

extern const AP_HAL::HAL& hal;

// by default use the airspeed pin for Vane
#define WINDVANE_DEFAULT_PIN 15
// use other analog pins for speed sensor by deault
#define WINDSPEED_DEFAULT_SPEED_PIN 14
#define WINDSPEED_DEFAULT_TEMP_PIN 13
// use average offset providey by manfacturer for wind sesnor rev P. as default
// https://moderndevice.com/news/calibrating-rev-p-wind-sensor-new-regression/
// will have to chage this once more sensors are supported
#define WINDSPEED_DEFAULT_VOLT_OFFSET 1.346

const AP_Param::GroupInfo AP_WindVane::var_info[] = {

    // @Param: TYPE
    // @DisplayName: Wind Vane Type
    // @Description: Wind Vane type
    // @Values: 0:None,1:Heading when armed,2:RC input offset heading when armed,3:Analog
    // @User: Standard
    AP_GROUPINFO_FLAGS("TYPE", 1, AP_WindVane, _type, 0, AP_PARAM_FLAG_ENABLE),

    // @Param: RC_IN_NO
    // @DisplayName: RC Input Channel to use as wind angle value
    // @Description: RC Input Channel to use as wind angle value
    // @Range: 0 16
    // @Increment: 1
    // @User: Standard
    AP_GROUPINFO("RC_IN_NO", 2, AP_WindVane, _rc_in_no, 0),

    // @Param: ANA_PIN
    // @DisplayName: Analog input
    // @Description: Analog input pin to read as Wind vane sensor pot
    // @Values: 11:Pixracer,13:Pixhawk ADC4,14:Pixhawk ADC3,15:Pixhawk ADC6,15:Pixhawk2 ADC,50:PixhawkAUX1,51:PixhawkAUX2,52:PixhawkAUX3,53:PixhawkAUX4,54:PixhawkAUX5,55:PixhawkAUX6,103:Pixhawk SBUS
    // @User: Standard
    AP_GROUPINFO("ANA_PIN", 3, AP_WindVane, _analog_pin_no, WINDVANE_DEFAULT_PIN),

    // @Param: ANA_V_MIN
    // @DisplayName: Analog minumum voltage
    // @Description: Minimum analalog voltage read by windvane
    // @Units: V
    // @Increment: 0.01
    // @Range: 0 5.0
    // @User: Standard
    AP_GROUPINFO("ANA_V_MIN", 4, AP_WindVane, _analog_volt_min, 0.0f),

    // @Param: ANA_V_MAX
    // @DisplayName: Analog maximum voltage
    // @Description: Minimum analalog voltage read by windvane
    // @Units: V
    // @Increment: 0.01
    // @Range: 0 5.0
    // @User: Standard
    AP_GROUPINFO("ANA_V_MAX", 5, AP_WindVane, _analog_volt_max, 3.3f),

    // @Param: ANA_OF_HD
    // @DisplayName: Analog headwind offset
    // @Description: Angle offset when windvane is indicating a headwind, ie 0 degress relative to vehicle
    // @Units: deg
    // @Increment: 1
    // @Range: 0 360
    // @User: Standard
    AP_GROUPINFO("ANA_OF_HD", 6, AP_WindVane, _analog_head_bearing_offset, 0.0f),

    // @Param: VANE_FILT
    // @DisplayName: Wind vane low pass filter frequency
    // @Description: Wind vane low pass filter frequency, a value of -1 disables filter
    // @Units: Hz
    // @User: Standard
    AP_GROUPINFO("VANE_FILT", 7, AP_WindVane, _vane_filt_hz, 0.5f),

    // @Param: CAL
    // @DisplayName: set to one to enter calibration on reboot
    // @Description: set to one to enter calibration on reboot
    // @Values: 0:None, 1:Calibrate
    // @User: Standard
    AP_GROUPINFO("CAL", 8, AP_WindVane, _calibration, 0),

    // @Param: ANA_DZ
    // @DisplayName: Analog potentiometer dead zone
    // @Description: Analog potentiometer mechanical dead zone
    // @Units: deg
    // @Increment: 1
    // @Range: 0 360
    // @User: Standard
    AP_GROUPINFO("ANA_DZ", 9, AP_WindVane, _analog_deadzone, 0),

    // @Param: CUTOFF
    // @DisplayName: Wind vane cut off wind speed
    // @Description: if a wind sensor is installed the wind vane will be ignored at apparent wind speeds bellow this, NOTE: if the apparent wind is consistantly bellow this the vane will not work
    // @Units: m/s
    // @Increment: 0.1
    // @Range: 0 5
    // @User: Standard
    AP_GROUPINFO("CUTOFF", 10, AP_WindVane, _apparent_wind_vane_cutoff, 0),

    // @Param: SPEED_TYPE
    // @DisplayName: Wind speed sensor Type
    // @Description: Wind Vane type
    // @Values: 0:None,1:Airspeed library,2:Moden Devices Wind Sensor rev. p
    // @User: Standard
    AP_GROUPINFO("SPEED_TYPE", 11, AP_WindVane, _wind_speed_sensor_type,  0),

    // @Param: SPEED_PIN1
    // @DisplayName: Analog speed sensor input 1
    // @Description: Wind speed analog speed input pin for Moden Devices Wind Sensor rev. p
    // @Values: 11:Pixracer,13:Pixhawk ADC4,14:Pixhawk ADC3,15:Pixhawk ADC6,15:Pixhawk2 ADC,50:PixhawkAUX1,51:PixhawkAUX2,52:PixhawkAUX3,53:PixhawkAUX4,54:PixhawkAUX5,55:PixhawkAUX6,103:Pixhawk SBUS
    // @User: Standard
    AP_GROUPINFO("SPEED_PIN1", 12, AP_WindVane, _wind_speed_sensor_speed_in,  WINDSPEED_DEFAULT_SPEED_PIN),

    // @Param: SPEED_PIN2
    // @DisplayName: Analog speed sensor input 2
    // @Description: Wind speed sensor analog temp input pin for Moden Devices Wind Sensor rev. p, set to -1 to diasble temp readings
    // @Values: 11:Pixracer,13:Pixhawk ADC4,14:Pixhawk ADC3,15:Pixhawk ADC6,15:Pixhawk2 ADC,50:PixhawkAUX1,51:PixhawkAUX2,52:PixhawkAUX3,53:PixhawkAUX4,54:PixhawkAUX5,55:PixhawkAUX6,103:Pixhawk SBUS
    // @User: Standard
    AP_GROUPINFO("SPEED_PIN2", 13, AP_WindVane, _wind_speed_sensor_temp_in,  WINDSPEED_DEFAULT_TEMP_PIN),

    // @Param: SPEED_OFS
    // @DisplayName: Analog speed zero wind voltage offset
    // @Description: Wind sensor analog voltage offset at zero wind speed
    // @Units: V
    // @Increment: 0.01
    // @Range: 0 3.3
    // @User: Standard
    AP_GROUPINFO("SPEED_OFS", 14, AP_WindVane, _wind_speed_sensor_voltage_offset, WINDSPEED_DEFAULT_VOLT_OFFSET),

    // @Param: SPEED_FILT
    // @DisplayName: Wind speed low pass filter frequency
    // @Description: Wind speed low pass filter frequency, a value of -1 disables filter
    // @Units: Hz
    // @User: Standard
    AP_GROUPINFO("SPEED_FILT", 15, AP_WindVane, _speed_filt_hz, 0.5f),

    AP_GROUPEND
};

// create a global instances of low pass filter
LowPassFilterFloat low_pass_filter_wind_sin =  LowPassFilterFloat(2.0f);
LowPassFilterFloat low_pass_filter_wind_cos =  LowPassFilterFloat(2.0f);
LowPassFilterFloat low_pass_filter_wind_speed =  LowPassFilterFloat(2.0f);

// Public
// ------

// constructor
AP_WindVane::AP_WindVane()
{
    AP_Param::setup_object_defaults(this, var_info);
    if (_s_instance) {
        AP_HAL::panic("Too many Wind Vane sensors");
    }
    _s_instance = this;
}

// destructor
AP_WindVane::~AP_WindVane(void)
{
}

/*
 * Get the AP_WindVane singleton
 */
AP_WindVane *AP_WindVane::get_instance()
{
    return _s_instance;
}

// return true if wind vane is enabled
bool AP_WindVane::enabled() const
{
    return (_type != WINDVANE_NONE);
}

// Initialize the Wind Vane object and prepare it for use
void AP_WindVane::init()
{
    // a pin for reading the Wind Vane voltage
    windvane_analog_source = hal.analogin->channel(ANALOG_INPUT_NONE);

    // pins for wind sensor rev p
    wind_speed_analog_source = hal.analogin->channel(ANALOG_INPUT_NONE);
    wind_speed_temp_analog_source = hal.analogin->channel(ANALOG_INPUT_NONE);

    // Link the airspeed libary
    _airspeed = AP_Airspeed::get_singleton();

    // Check that airspeed is enabled if it is slected as sensor type, if not revert to no wind speed sensor
    if (_wind_speed_sensor_type == Speed_type::WINDSPEED_AIRSPEED && (_airspeed == nullptr || !_airspeed->enabled())) {
        _wind_speed_sensor_type.set(Speed_type::WINDSPEED_NONE);
    }
}

// Update wind vane, called at 10hz
void AP_WindVane::update()
{
    // exit immediately if not enabled
    if (!enabled()) {
        return;
    }

    // check for calibration
    calibrate();

    update_wind_speed();
    update_apparent_wind_direction();
    update_true_wind_direction();
}

// record home heading for use as wind direction if no sensor is fitted
void AP_WindVane::record_home_headng()
{
    _home_heading = AP::ahrs().yaw;
}

// Private
// -------

// read the Wind Vane value from an analog pin, calculate bearing from analog voltage
// assumes voltage increases as wind vane moves clockwise
float AP_WindVane::read_analog()
{
    windvane_analog_source->set_pin(_analog_pin_no);
    _current_analog_voltage = windvane_analog_source->voltage_average();

    float current_analog_voltage_constrain = constrain_float(_current_analog_voltage,_analog_volt_min,_analog_volt_max);
    float voltage_ratio = linear_interpolate(0.0f, 1.0f, current_analog_voltage_constrain, _analog_volt_min, _analog_volt_max);

    float bearing = (voltage_ratio * radians(360 - _analog_deadzone)) + radians(_analog_head_bearing_offset);

    return wrap_PI(bearing);
}

// read the bearing value from a PWM value on a RC channel (+- 45deg)
float AP_WindVane::read_PWM_bearing()
{
    RC_Channel *ch = rc().channel(_rc_in_no-1);
    if (ch == nullptr) {
        return 0.0f;
    }
    float bearing = ch->norm_input() * radians(45);

    return wrap_PI(bearing);
}

// read the apparent wind direction in radians from SITL
float AP_WindVane::read_direction_SITL()
{
#if CONFIG_HAL_BOARD == HAL_BOARD_SITL
    // temporarily store true speed and direction for easy access
    const float wind_speed = AP::sitl()->wind_speed_active;
    const float wind_dir_rad = radians(AP::sitl()->wind_direction_active);

    // convert true wind speed and direction into a 2D vector
    Vector2f wind_vector_ef(sinf(wind_dir_rad) * wind_speed, cosf(wind_dir_rad) * wind_speed);

    // add vehicle speed to get apparent wind vector
    wind_vector_ef.x += AP::sitl()->state.speedE;
    wind_vector_ef.y += AP::sitl()->state.speedN;

    const float wind_dir_apparent = wrap_PI(atan2f(wind_vector_ef.x, wind_vector_ef.y) - radians(AP::sitl()->state.heading));

    // debug
    /*printf("wx:%4.2f wy:%4.2f wda:%4.2f wsa:%4.2f wspd:%4.2f wdir:%4.2f\n",
                (double)wind_vector_ef.x,
                (double)wind_vector_ef.y,
                (double)degrees(wind_dir_rad),
                (double)wind_speed,
                (double)wind_vector_ef.length(),
                (double)degrees(wind_dir_apparent)
                );*/

    return wind_dir_apparent;
#else
    return 0.0f;
#endif
}

// read the apparent wind speed in m/s from SITL
float AP_WindVane::read_wind_speed_SITL()
{
#if CONFIG_HAL_BOARD == HAL_BOARD_SITL
    // temporarily store true speed and direction for easy access
    const float wind_speed = AP::sitl()->wind_speed_active;
    const float wind_dir_rad = radians(AP::sitl()->wind_direction_active);

    // convert true wind speed and direction into a 2D vector
    Vector2f wind_vector_ef(sinf(wind_dir_rad) * wind_speed, cosf(wind_dir_rad) * wind_speed);

    // add vehicle speed to get apparent wind vector
    wind_vector_ef.x += AP::sitl()->state.speedE;
    wind_vector_ef.y += AP::sitl()->state.speedN;

    return wind_vector_ef.length();
#else
    return 0.0f;
#endif
}

// read modern devices wind sensor rev p
// https://moderndevice.com/news/calibrating-rev-p-wind-sensor-new-regression/
float AP_WindVane::read_wind_sensor_rev_p()
{
    float analog_voltage = 0.0f;

    // only read temp pin if defined, sensor will do OK assuming constant temp
    float t_ambient = 28.0f; // assume room temp (deg c), equations were generated at this temp in above data sheet
    if (is_positive(_wind_speed_sensor_temp_in)) {
        wind_speed_temp_analog_source->set_pin(_wind_speed_sensor_temp_in);
        analog_voltage = wind_speed_temp_analog_source->voltage_average();
        t_ambient = (analog_voltage - 0.4f) / 0.0195f; // deg c
        // constrain to reasonable range to avoid deviating from calabration too much and potential devide by zero
        t_ambient = constrain_float(t_ambient, 10.0f, 40.0f);
    }

    wind_speed_analog_source->set_pin(_wind_speed_sensor_speed_in);
    analog_voltage = wind_speed_analog_source->voltage_average();

    // Apply voltage offset and make sure not negative
    analog_voltage = analog_voltage - _wind_speed_sensor_voltage_offset;
    if (is_negative(analog_voltage)) {
        analog_voltage = 0.0f;
    }

    // Simplified equation from data sheet multiplied by mph to m/s conversion
    return 24.254896f * powf((analog_voltage / powf(t_ambient,0.115157f)) ,3.009364f);
}

// Update the apparent wind speed
void AP_WindVane::update_wind_speed()
{
    float apparent_wind_speed_in = 0.0f;

    switch (_wind_speed_sensor_type) {
        case WINDSPEED_AIRSPEED:
            apparent_wind_speed_in = _airspeed->get_airspeed();
            break;
        case WINDVANE_WIND_SENSOR_REV_P:
            apparent_wind_speed_in = read_wind_sensor_rev_p();
            break;
        case WINDSPEED_SITL:
#if CONFIG_HAL_BOARD == HAL_BOARD_SITL
            apparent_wind_speed_in = read_wind_speed_SITL();
            break;
#endif
        default:
            _speed_apparent = 0.0f;
            return;
    }

    // apply low pass filter if enabled
    if (is_positive(_speed_filt_hz)) {
        low_pass_filter_wind_speed.set_cutoff_frequency(_speed_filt_hz);
        _speed_apparent = low_pass_filter_wind_speed.apply(apparent_wind_speed_in, 0.02f);
    } else {
        _speed_apparent = apparent_wind_speed_in;
    }
}

// Calculate the apparent wind direction in radians, the wind comes from this direction, 0 = head to wind
void AP_WindVane::update_apparent_wind_direction()
{
    float apparent_angle_in = 0.0f;

    switch (_type) {
        case WindVaneType::WINDVANE_HOME_HEADING:
            // this is a approximation as we are not considering boat speed and wind speed
            // do not filter home heading
            _direction_apparent = wrap_PI(_home_heading - AP::ahrs().yaw);
            return;
        case WindVaneType::WINDVANE_PWM_PIN:
            // this is a approximation as we are not considering boat speed and wind speed
            // do not filter home heading and pwm type vanes
            _direction_apparent = wrap_PI(read_PWM_bearing() + _home_heading - AP::ahrs().yaw);
            return;
        case WindVaneType::WINDVANE_ANALOG_PIN:
            apparent_angle_in = read_analog();
            break;
        case WindVaneType::WINDVANE_SITL:
#if CONFIG_HAL_BOARD == HAL_BOARD_SITL
            apparent_angle_in = read_direction_SITL();
#endif
            break;
    }

    // if not enough wind to move vane do not update the value
    if (_speed_apparent < _apparent_wind_vane_cutoff){
        return;
    }

    // apply low pass filter if enabled
    if (is_positive(_vane_filt_hz)) {
        low_pass_filter_wind_sin.set_cutoff_frequency(_vane_filt_hz);
        low_pass_filter_wind_cos.set_cutoff_frequency(_vane_filt_hz);
        // https://en.wikipedia.org/wiki/Mean_of_circular_quantities
        float filtered_sin = low_pass_filter_wind_sin.apply(sinf(apparent_angle_in), 0.02f);
        float filtered_cos = low_pass_filter_wind_cos.apply(cosf(apparent_angle_in), 0.02f);
        _direction_apparent = atan2f(filtered_sin, filtered_cos);
    } else {
        _direction_apparent = apparent_angle_in;
    }

    // make sure between -pi and pi
    _direction_apparent = wrap_PI(_direction_apparent);
}

// convert from apparent wind angle to true wind absolute angle and true wind speed
// https://en.wikipedia.org/wiki/Apparent_wind
void AP_WindVane::update_true_wind_direction()
{
    float heading = AP::ahrs().yaw;

    // no wind speed sensor, so can't do true wind calcs
    if (_wind_speed_sensor_type == Speed_type::WINDSPEED_NONE) {
        _direction_absolute = wrap_2PI(heading + _direction_apparent);
        return;
    }

    // duplicated from rover get forward speed
    float ground_speed = 0.0f;
    Vector3f velocity;
    if (!AP::ahrs().get_velocity_NED(velocity)) {
        // use less accurate GPS, assuming entire length is along forward/back axis of vehicle
        if (AP::gps().status() >= AP_GPS::GPS_OK_FIX_3D) {
            if (labs(wrap_180_cd(AP::ahrs().yaw_sensor - AP::gps().ground_course_cd())) <= 9000) {
                ground_speed = AP::gps().ground_speed();
            } else {
                ground_speed = -AP::gps().ground_speed();
            }
        }
    }
    // calculate forward speed velocity in body frame
    ground_speed = velocity.x*AP::ahrs().cos_yaw() + velocity.y*AP::ahrs().sin_yaw();

    // update true wind speed
    _speed_true = sqrtf(powf(_speed_apparent, 2) + powf(ground_speed, 2) - 2 * _speed_apparent * ground_speed * cosf(_direction_apparent));

    float bearing = 0.0f;
    if (is_zero(_speed_true)) { // no wind so ignore apparent wind effects
        bearing = _direction_apparent;
    } else if (is_positive(_direction_apparent)) {
        bearing = acosf((_speed_apparent * cosf(_direction_apparent) - ground_speed) / _speed_true);
    } else {
        // To-Do: check if arg to acosf > 1 to avoid arithmetic exception
        bearing = -acosf((_speed_apparent * cosf(_direction_apparent) - ground_speed) / _speed_true);
    }

    // make sure between -pi and pi
    _direction_absolute = wrap_2PI(heading + bearing);
}

// calibrate windvane
void AP_WindVane::calibrate()
{
    // exit immediately if armed or too soon after start
    if (hal.util->get_soft_armed()) {
        return;
    }

    // return if not calibrating
    if (_calibration == 0) {
        return;
    }

    switch (_type) {
        case WindVaneType::WINDVANE_HOME_HEADING:
        case WindVaneType::WINDVANE_PWM_PIN:
            gcs().send_text(MAV_SEVERITY_INFO, "WindVane: No cal required");
            _calibration.set_and_save(0);
            break;
        case WindVaneType::WINDVANE_ANALOG_PIN:
            // start calibration
            if (_cal_start_ms == 0) {
                _cal_start_ms = AP_HAL::millis();
                _cal_volt_max = _current_analog_voltage;
                _cal_volt_min = _current_analog_voltage;
                gcs().send_text(MAV_SEVERITY_INFO, "WindVane: Analog input calibrating");
            }

            // record min and max voltage
            _cal_volt_max = fmaxf(_cal_volt_max, _current_analog_voltage);
            _cal_volt_min = fminf(_cal_volt_min, _current_analog_voltage);

            // calibrate for 30 seconds
            if ((AP_HAL::millis() - _cal_start_ms) > 30000) {
                // save min and max voltage
                _analog_volt_max.set_and_save(_cal_volt_max);
                _analog_volt_min.set_and_save(_cal_volt_min);
                _calibration.set_and_save(0);
                _cal_start_ms = 0;
                gcs().send_text(MAV_SEVERITY_INFO, "WindVane: Analog input calibration complete");
            }
            break;
    }
}

AP_WindVane *AP_WindVane::_s_instance = nullptr;

namespace AP {
    AP_WindVane *windvane()
    {
        return AP_WindVane::get_instance();
    }
};
