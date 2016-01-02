// -*- tab-width: 4; Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*-
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

/*
 *       AP_MotorsTri.cpp - ArduCopter motors library
 *       Code by RandyMackay. DIYDrones.com
 *
 */
#include <AP_HAL/AP_HAL.h>
#include <AP_Math/AP_Math.h>
#include "AP_MotorsTri.h"

extern const AP_HAL::HAL& hal;

const AP_Param::GroupInfo AP_MotorsTri::var_info[] = {
    // variables from parent vehicle
    AP_NESTEDGROUPINFO(AP_MotorsMulticopter, 0),

    // parameters 1 ~ 29 were reserved for tradheli
    // parameters 30 ~ 39 reserved for tricopter
    // parameters 40 ~ 49 for single copter and coax copter (these have identical parameter files)

    // @Param: YAW_SV_REV
    // @DisplayName: Yaw Servo Reverse
    // @Description: Yaw servo reversing. Set to 1 for normal (forward) operation. Set to -1 to reverse this channel.
    // @Values: -1:Reversed,1:Normal
    // @User: Standard
    AP_GROUPINFO("YAW_SV_REV", 31,     AP_MotorsTri,  _yaw_servo_reverse, 1),

    // @Param: YAW_SV_TRIM
    // @DisplayName: Yaw Servo Trim/Center
    // @Description: Trim or center position of yaw servo
    // @Range: 1250 1750
    // @Units: PWM
    // @Increment: 1
    // @User: Standard
    AP_GROUPINFO("YAW_SV_TRIM", 32,     AP_MotorsTri,  _yaw_servo_trim, 1500),

    // @Param: YAW_SV_MIN
    // @DisplayName: Yaw Servo Min Position
    // @Description: Minimum angle limit of yaw servo
    // @Range: 1000 1400
    // @Units: PWM
    // @Increment: 1
    // @User: Standard
    AP_GROUPINFO("YAW_SV_MIN", 33,     AP_MotorsTri,  _yaw_servo_min, 1250),

    // @Param: YAW_SV_MAX
    // @DisplayName: Yaw Servo Max Position
    // @Description: Maximum angle limit of yaw servo
    // @Range: 1600 2000
    // @Units: PWM
    // @Increment: 1
    // @User: Standard
    AP_GROUPINFO("YAW_SV_MAX", 34,     AP_MotorsTri,  _yaw_servo_max, 1750),


    AP_GROUPEND
};

// init
void AP_MotorsTri::Init()
{
    // set update rate for the 3 motors (but not the servo on channel 7)
    set_update_rate(_speed_hz);

    // set the motor_enabled flag so that the ESCs can be calibrated like other frame types
    motor_enabled[AP_MOTORS_MOT_1] = true;
    motor_enabled[AP_MOTORS_MOT_2] = true;
    motor_enabled[AP_MOTORS_MOT_4] = true;

    // disable CH7 from being used as an aux output (i.e. for camera gimbal, etc)
    RC_Channel_aux::disable_aux_channel(AP_MOTORS_CH_TRI_YAW);
}

// set update rate to motors - a value in hertz
void AP_MotorsTri::set_update_rate( uint16_t speed_hz )
{
    // record requested speed
    _speed_hz = speed_hz;

    // set update rate for the 3 motors (but not the servo on channel 7)
    uint32_t mask = 
	    1U << AP_MOTORS_MOT_1 |
	    1U << AP_MOTORS_MOT_2 |
	    1U << AP_MOTORS_MOT_4;
    hal.rcout->set_freq(mask, _speed_hz);
}

// enable - starts allowing signals to be sent to motors
void AP_MotorsTri::enable()
{
    // enable output channels
    hal.rcout->enable_ch(AP_MOTORS_MOT_1);
    hal.rcout->enable_ch(AP_MOTORS_MOT_2);
    hal.rcout->enable_ch(AP_MOTORS_MOT_4);
    hal.rcout->enable_ch(AP_MOTORS_CH_TRI_YAW);
}

// output_min - sends minimum values out to the motors
void AP_MotorsTri::output_min()
{
    // send minimum value to each motor
    hal.rcout->cork();
    hal.rcout->write(AP_MOTORS_MOT_1, _throttle_radio_min);
    hal.rcout->write(AP_MOTORS_MOT_2, _throttle_radio_min);
    hal.rcout->write(AP_MOTORS_MOT_4, _throttle_radio_min);
    hal.rcout->write(AP_MOTORS_CH_TRI_YAW, _yaw_servo_trim);
    hal.rcout->push();
}

void AP_MotorsTri::output_to_motors()
{
    int8_t i;

    if (!armed()){
        _multicopter_flags.spool_mode = SHUT_DOWN;
    }
    switch (_multicopter_flags.spool_mode) {
        case SHUT_DOWN:
            // sends minimum values out to the motors
            hal.rcout->cork();
            hal.rcout->write(AP_MOTORS_MOT_1, _throttle_radio_min);
            hal.rcout->write(AP_MOTORS_MOT_2, _throttle_radio_min);
            hal.rcout->write(AP_MOTORS_MOT_4, _throttle_radio_min);
            hal.rcout->write(AP_MOTORS_CH_TRI_YAW, _yaw_servo_trim);
            hal.rcout->push();
            break;
        case SPIN_WHEN_ARMED:
            // sends output to motors when armed but not flying
            hal.rcout->cork();
            hal.rcout->write(AP_MOTORS_MOT_1, constrain_int16(_throttle_radio_min + _throttle_low_end_pct * _min_throttle, _throttle_radio_min, _throttle_radio_min + _min_throttle));
            hal.rcout->write(AP_MOTORS_MOT_2, constrain_int16(_throttle_radio_min + _throttle_low_end_pct * _min_throttle, _throttle_radio_min, _throttle_radio_min + _min_throttle));
            hal.rcout->write(AP_MOTORS_MOT_4, constrain_int16(_throttle_radio_min + _throttle_low_end_pct * _min_throttle, _throttle_radio_min, _throttle_radio_min + _min_throttle));
            hal.rcout->write(AP_MOTORS_CH_TRI_YAW, _yaw_servo_trim);
            hal.rcout->push();
            break;
        case SPOOL_UP:
        case THROTTLE_UNLIMITED:
        case SPOOL_DOWN:
            // set motor output based on thrust requests
            hal.rcout->cork();
            hal.rcout->write(AP_MOTORS_MOT_1, calc_thrust_to_pwm(_thrust_rpyt_out[AP_MOTORS_MOT_1]));
            hal.rcout->write(AP_MOTORS_MOT_2, calc_thrust_to_pwm(_thrust_rpyt_out[AP_MOTORS_MOT_2]));
            hal.rcout->write(AP_MOTORS_MOT_4, calc_thrust_to_pwm(_thrust_rpyt_out[AP_MOTORS_MOT_4]));
            hal.rcout->write(AP_MOTORS_CH_TRI_YAW, calc_yaw_radio_output(_pivot_angle, radians(30.0f)));
            hal.rcout->push();
            break;
    }
}

// get_motor_mask - returns a bitmask of which outputs are being used for motors or servos (1 means being used)
//  this can be used to ensure other pwm outputs (i.e. for servos) do not conflict
uint16_t AP_MotorsTri::get_motor_mask()
{
    // tri copter uses channels 1,2,4 and 7
    return (1U << AP_MOTORS_MOT_1) |
        (1U << AP_MOTORS_MOT_2) |
        (1U << AP_MOTORS_MOT_4) |
        (1U << AP_MOTORS_CH_TRI_YAW);
}

// output_armed - sends commands to the motors
// includes new scaling stability patch
// TODO pull code that is common to output_armed_not_stabilizing into helper functions
void AP_MotorsTri::output_armed_stabilizing()
{
    float   roll_thrust;                // roll thrust value, initially calculated by calc_roll_thrust() but may be modified after, +/- 1.0
    float   pitch_thrust;               // pitch thrust value, initially calculated by calc_roll_thrust() but may be modified after, +/- 1.0
    float   yaw_thrust;                 // yaw thrust value, initially calculated by calc_yaw_thrust() but may be modified after, +/- 1.0
    float   throttle_thrust;            // throttle thrust value, summed onto throttle channel minimum, 0.0 - 1.0
    float   throttle_thrust_best_rpy;   // throttle providing maximum roll, pitch and yaw range without climbing
    float   rpy_scale = 1.0f;           // this is used to scale the roll, pitch and yaw to fit within the motor limits
    float   rpy_low = 0.0f;             // lowest motor value
    float   rpy_high = 0.0f;            // highest motor value
    float   thr_adj;                    // the difference between the pilot's desired throttle and throttle_thrust_best_rpy

    // apply voltage and air pressure compensation
    roll_thrust = get_roll_thrust() * get_compensation_gain();
    pitch_thrust = get_pitch_thrust() * get_compensation_gain();
    yaw_thrust = get_yaw_thrust() * get_compensation_gain();
    throttle_thrust = get_throttle_thrust() * get_compensation_gain();
    float pivot_angle_max = asin(yaw_thrust);
    float pivot_thrust_max = cos(pivot_angle_max);
    float thrust_max = 1.0f;

    // sanity check throttle is above zero and below current limited throttle
    if (throttle_thrust <= 0.0f) {
        throttle_thrust = 0.0f;
        limit.throttle_lower = true;
    }
    // convert throttle_max from 0~1000 to 0~1 range
    if (throttle_thrust >= _throttle_thrust_max) {
        throttle_thrust = _throttle_thrust_max;
        limit.throttle_upper = true;
    }

    _thrust_rpyt_out[AP_MOTORS_MOT_1] = roll_thrust * -0.5f + pitch_thrust * 1.0f;
    _thrust_rpyt_out[AP_MOTORS_MOT_2] = roll_thrust * 0.5f + pitch_thrust * 1.0f;
    _thrust_rpyt_out[AP_MOTORS_MOT_4] = 0;

    _thrust_rpyt_out[AP_MOTORS_MOT_1] = roll_thrust * -0.866f + pitch_thrust * 0.5f;
    _thrust_rpyt_out[AP_MOTORS_MOT_2] = roll_thrust * 0.866f + pitch_thrust * 0.5f;
    _thrust_rpyt_out[AP_MOTORS_MOT_4] = pitch_thrust * -0.5f;

    // calculate roll and pitch for each motor
    // set rpy_low and rpy_high to the lowest and highest values of the motors

    // record lowest roll pitch command
    rpy_low = MIN(_thrust_rpyt_out[AP_MOTORS_MOT_1],_thrust_rpyt_out[AP_MOTORS_MOT_2]);
    rpy_high = MAX(_thrust_rpyt_out[AP_MOTORS_MOT_1],_thrust_rpyt_out[AP_MOTORS_MOT_2]);
    if (rpy_low > _thrust_rpyt_out[AP_MOTORS_MOT_4]){
        rpy_low = _thrust_rpyt_out[AP_MOTORS_MOT_4];
    }
    if ((1.0f -rpy_high) > (pivot_thrust_max-_thrust_rpyt_out[AP_MOTORS_MOT_4])){
        thrust_max = pivot_thrust_max;
        rpy_high = _thrust_rpyt_out[AP_MOTORS_MOT_4];
    }

    // calculate throttle that gives most possible room for yaw (range 1000 ~ 2000) which is the lower of:
    //      1. 0.5f - (rpy_low+rpy_high)/2.0 - this would give the maximum possible room margin above the highest motor and below the lowest
    //      2. the higher of:
    //            a) the pilot's throttle input
    //            b) the point _throttle_rpy_mix between the pilot's input throttle and hover-throttle
    //      Situation #2 ensure we never increase the throttle above hover throttle unless the pilot has commanded this.
    //      Situation #2b allows us to raise the throttle above what the pilot commanded but not so far that it would actually cause the copter to rise.
    //      We will choose #1 (the best throttle for yaw control) if that means reducing throttle to the motors (i.e. we favour reducing throttle *because* it provides better yaw control)
    //      We will choose #2 (a mix of pilot and hover throttle) only when the throttle is quite low.  We favour reducing throttle instead of better yaw control because the pilot has commanded it

    float throttle_thrust_hover = get_hover_throttle_as_high_end_pct();
    throttle_thrust_best_rpy = MIN(0.5f*thrust_max - (rpy_low+rpy_high)/2.0, MAX(throttle_thrust, throttle_thrust*MAX(0.0f,1.0f-_throttle_rpy_mix)+throttle_thrust_hover*_throttle_rpy_mix));

    // check everything fits
    thr_adj = throttle_thrust - throttle_thrust_best_rpy;

    // calculate upper and lower limits of thr_adj
    float thr_adj_max = MAX(thrust_max-(throttle_thrust_best_rpy+rpy_high),0.0f);

    // if we are increasing the throttle (situation #2 above)..
    if (thr_adj > 0.0f) {
        // increase throttle as close as possible to requested throttle
        // without going over 1.0f
        if (thr_adj > thr_adj_max){
            thr_adj = thr_adj_max;
            // we haven't even been able to apply full throttle command
            limit.throttle_upper = true;
        }
    }else if(thr_adj < 0){
        // decrease throttle as close as possible to requested throttle
        // without going under 0.0f or over 1.0f
        // earlier code ensures we can't break both boundaries
        float thr_adj_min = MIN(-(throttle_thrust_best_rpy+rpy_low),0.0f);
        if (thr_adj > thr_adj_max) {
            thr_adj = thr_adj_max;
            limit.throttle_upper = true;
        }
        if (thr_adj < thr_adj_min) {
            thr_adj = thr_adj_min;
        }
    }

    // do we need to reduce roll, pitch, yaw command
    // earlier code does not allow both limit's to be passed simultaneously with abs(_yaw_factor)<1
    if ((rpy_low+throttle_thrust_best_rpy)+thr_adj < 0.0f){
        // protect against divide by zero
        if (!is_zero(rpy_low)) {
            rpy_scale = -(thr_adj+throttle_thrust_best_rpy)/rpy_low;
        }
        // we haven't even been able to apply full roll, pitch and minimal yaw without scaling
        limit.roll_pitch = true;
        limit.yaw = true;
    }else if((rpy_high+throttle_thrust_best_rpy)+thr_adj > 1.0f){
        // protect against divide by zero
        if (!is_zero(rpy_high)) {
            rpy_scale = (1.0f-thr_adj-throttle_thrust_best_rpy)/rpy_high;
        }
        // we haven't even been able to apply full roll, pitch and minimal yaw without scaling
        limit.roll_pitch = true;
        limit.yaw = true;
    }

    // add scaled roll, pitch, constrained yaw and throttle for each motor
    _thrust_rpyt_out[AP_MOTORS_MOT_1] = throttle_thrust_best_rpy+thr_adj + rpy_scale*_thrust_rpyt_out[AP_MOTORS_MOT_1];
    _thrust_rpyt_out[AP_MOTORS_MOT_2] = throttle_thrust_best_rpy+thr_adj + rpy_scale*_thrust_rpyt_out[AP_MOTORS_MOT_2];
    _thrust_rpyt_out[AP_MOTORS_MOT_4] = throttle_thrust_best_rpy+thr_adj + rpy_scale*_thrust_rpyt_out[AP_MOTORS_MOT_4];

    // calculate angle of yaw pivot
    _pivot_angle = atan(yaw_thrust/_thrust_rpyt_out[AP_MOTORS_MOT_4]);
    // scale pivot thrust to account for pivot angle
    _thrust_rpyt_out[AP_MOTORS_MOT_4] = _thrust_rpyt_out[AP_MOTORS_MOT_4]/cos(_pivot_angle);
}

// output_disarmed - sends commands to the motors
void AP_MotorsTri::output_disarmed()
{
    // Send minimum values to all motors
    output_min();
}

// output_test - spin a motor at the pwm value specified
//  motor_seq is the motor's sequence number from 1 to the number of motors on the frame
//  pwm value is an actual pwm value that will be output, normally in the range of 1000 ~ 2000
void AP_MotorsTri::output_test(uint8_t motor_seq, int16_t pwm)
{
    // exit immediately if not armed
    if (!armed()) {
        return;
    }

    // output to motors and servos
    switch (motor_seq) {
        case 1:
            // front right motor
            hal.rcout->write(AP_MOTORS_MOT_1, pwm);
            break;
        case 2:
            // back motor
            hal.rcout->write(AP_MOTORS_MOT_4, pwm);
            break;
        case 3:
            // back servo
            hal.rcout->write(AP_MOTORS_CH_TRI_YAW, pwm);
            break;
        case 4:
            // front left motor
            hal.rcout->write(AP_MOTORS_MOT_2, pwm);
            break;
        default:
            // do nothing
            break;
    }
}

// calc_yaw_radio_output - calculate final radio output for yaw channel
int16_t AP_MotorsTri::calc_yaw_radio_output(float yaw_input, float yaw_input_max)
{
    int16_t ret;

    if (_yaw_servo_reverse < 0) {
        if (yaw_input >= 0){
            ret = (_yaw_servo_trim - (yaw_input/yaw_input_max * (_yaw_servo_trim - _yaw_servo_min)));
        } else {
            ret = (_yaw_servo_trim - (yaw_input/yaw_input_max * (_yaw_servo_max - _yaw_servo_trim)));
        }
    } else {
        if (yaw_input >= 0){
            ret = ((yaw_input/yaw_input_max * (_yaw_servo_max - _yaw_servo_trim)) + _yaw_servo_trim);
        } else {
            ret = ((yaw_input/yaw_input_max * (_yaw_servo_trim - _yaw_servo_min)) + _yaw_servo_trim);
        }
    }

    return ret;
}
