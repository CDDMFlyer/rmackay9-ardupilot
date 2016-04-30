#pragma once

// this class is #included into the Copter:: namespace

class FlightController {
    friend class Copter;

public:

    FlightController(Copter &copter) :
        _copter(copter),
        g(copter.g),
        wp_nav(_copter.wp_nav),
        pos_control(_copter.pos_control),
        inertial_nav(_copter.inertial_nav),
        ahrs(_copter.ahrs),
        attitude_control(_copter.attitude_control),
        motors(_copter.motors),
        channel_roll(_copter.channel_roll),
        channel_pitch(_copter.channel_pitch),
        channel_throttle(_copter.channel_throttle),
        channel_yaw(_copter.channel_yaw),
        ap(_copter.ap)
        { };

protected:

    virtual bool init(bool ignore_checks) = 0; // should be called at 100hz or more
    virtual void run() = 0; // should be called at 100hz or more

    virtual bool is_autopilot() const { return false; }
    virtual bool requires_GPS() const = 0;
    virtual bool has_manual_throttle() const = 0;
    virtual bool allows_arming(bool from_gcs) const = 0;
    void print_FlightMode(AP_HAL::BetterStream *port) const {
        port->print(name());
    }
    virtual const char *name() const = 0;

    Copter &_copter;

    // convenience references to avoid code churn in conversion:
    Parameters &g;
    AC_WPNav &wp_nav;
    AC_PosControl &pos_control;
    AP_InertialNav &inertial_nav;
    AP_AHRS &ahrs;
    AC_AttitudeControl_t &attitude_control;
    MOTOR_CLASS &motors;
    RC_Channel *&channel_roll;
    RC_Channel *&channel_pitch;
    RC_Channel *&channel_throttle;
    RC_Channel *&channel_yaw;
    ap_t &ap;

    // pass-through functions to reduce code churn on conversion;
    // these are candidates for moving into the FlightController base
    // class.
    virtual float get_throttle_pre_takeoff(float input_thr) {
        return _copter.get_throttle_pre_takeoff(input_thr);
    }
    virtual void get_pilot_desired_lean_angles(float roll_in, float pitch_in, float &roll_out, float &pitch_out, float angle_max) {
        _copter.get_pilot_desired_lean_angles(roll_in, pitch_in, roll_out, pitch_out, angle_max);
    }
    virtual float get_surface_tracking_climb_rate(int16_t target_rate, float current_alt_target, float dt) {
        return _copter.get_surface_tracking_climb_rate(target_rate, current_alt_target, dt);
    }
    virtual float get_pilot_desired_yaw_rate(int16_t stick_angle) {
        return _copter.get_pilot_desired_yaw_rate(stick_angle);
    }
    virtual float get_pilot_desired_climb_rate(float throttle_control) {
        return _copter.get_pilot_desired_climb_rate(throttle_control);
    }
    virtual float get_pilot_desired_throttle(int16_t throttle_control) {
        return _copter.get_pilot_desired_throttle(throttle_control);
    }
    virtual void update_simple_mode(void) {
        _copter.update_simple_mode();
    }
    virtual float get_smoothing_gain() {
        return _copter.get_smoothing_gain();
    }
    // end pass-through functions
};


class FlightController_ACRO : public FlightController {

public:

    FlightController_ACRO(Copter &copter) :
        Copter::FlightController(copter)
        { }
    virtual bool init(bool ignore_checks) override;
    virtual void run() override; // should be called at 100hz or more

    virtual bool is_autopilot() const override { return false; }
    virtual bool requires_GPS() const override { return false; }
    virtual bool has_manual_throttle() const override { return true; }
    virtual bool allows_arming(bool from_gcs) const override { return true; };

protected:

    const char *name() const override { return "ACRO"; }

    void get_pilot_desired_angle_rates(int16_t roll_in, int16_t pitch_in, int16_t yaw_in, float &roll_out, float &pitch_out, float &yaw_out);

private:

};


#if FRAME_CONFIG == HELI_FRAME
class FlightController_ACRO_Heli : public FlightController_ACRO {

public:

    FlightController_ACRO_Heli(Copter &copter) :
        Copter::FlightController_ACRO(copter)
        { }

    bool init(bool ignore_checks) override;
    void run() override; // should be called at 100hz or more

protected:
private:
};
#endif


class FlightController_ALTHOLD : public FlightController {

public:

    FlightController_ALTHOLD(Copter &copter) :
        Copter::FlightController(copter)
        { }

    bool init(bool ignore_checks) override;
    void run() override; // should be called at 100hz or more

    bool requires_GPS() const override { return false; }
    bool has_manual_throttle() const override { return false; }
    bool allows_arming(bool from_gcs) const override { return true; };
    bool is_autopilot() const override { return false; }

protected:

    const char *name() const override { return "ALT_HOLD"; }

private:

};


class FlightController_AUTO : public FlightController {

public:

    FlightController_AUTO(Copter &copter) :
        Copter::FlightController(copter)
        { }

    virtual bool init(bool ignore_checks) override;
    virtual void run() override; // should be called at 100hz or more

    virtual bool is_autopilot() const override { return true; }
    virtual bool requires_GPS() const override { return true; }
    virtual bool has_manual_throttle() const override { return false; }
    virtual bool allows_arming(bool from_gcs) const override { return false; };

    // Auto
    AutoMode mode() { return _mode; }

    bool loiter_start();
    void rtl_start();
    void takeoff_start(const Location& dest_loc);
    void wp_start(const Vector3f& destination);
    void wp_start(const Location_Class& dest_loc);
    void land_start();
    void land_start(const Vector3f& destination);
    void circle_movetoedge_start(const Location_Class &circle_center, float radius_m);
    void circle_start();
    void spline_start(const Location_Class& destination, bool stopped_at_start, AC_WPNav::spline_segment_end_type seg_end_type, const Location_Class& next_spline_destination);
    void nav_guided_start();

    bool landing_gear_should_be_deployed();

protected:

    const char *name() const override { return "AUTO"; }

//    void get_pilot_desired_angle_rates(int16_t roll_in, int16_t pitch_in, int16_t yaw_in, float &roll_out, float &pitch_out, float &yaw_out);

private:

    void takeoff_run();
    void wp_run();
    void spline_run();
    void land_run();
    void rtl_run();
    void circle_run();
    void nav_guided_run();
    void loiter_run();

    AutoMode _mode = Auto_TakeOff;   // controls which auto controller is run

};

#if AUTOTUNE_ENABLED == ENABLED
class FlightController_AUTOTUNE : public FlightController {

public:

    FlightController_AUTOTUNE(Copter &copter) :
        Copter::FlightController(copter)
        { }

    bool init(bool ignore_checks) override;
    void run() override; // should be called at 100hz or more

    bool requires_GPS() const override { return false; }
    bool has_manual_throttle() const override { return false; }
    bool allows_arming(bool from_gcs) const override { return false; };
    bool is_autopilot() const override { return false; }

    float get_autotune_descent_speed();
    bool autotuneing_with_GPS();
    void do_not_use_GPS();

    void autotune_stop();
    void autotune_save_tuning_gains();

protected:

    const char *name() const override { return "AUTOTUNE"; }

private:

    bool autotune_start(bool ignore_checks);
    void autotune_attitude_control();
    void autotune_backup_gains_and_initialise();
    void autotune_load_orig_gains();
    void autotune_load_tuned_gains();
    void autotune_load_intra_test_gains();
    void autotune_load_twitch_gains();
    void autotune_update_gcs(uint8_t message_id);
    bool autotune_roll_enabled();
    bool autotune_pitch_enabled();
    bool autotune_yaw_enabled();
    void autotune_twitching_test(float measurement, float target, float &measurement_min, float &measurement_max);
    void autotune_updating_d_up(float &tune_d, float tune_d_min, float tune_d_max, float tune_d_step_ratio, float &tune_p, float tune_p_min, float tune_p_max, float tune_p_step_ratio, float target, float measurement_min, float measurement_max);
    void autotune_updating_d_down(float &tune_d, float tune_d_min, float tune_d_step_ratio, float &tune_p, float tune_p_min, float tune_p_max, float tune_p_step_ratio, float target, float measurement_min, float measurement_max);
    void autotune_updating_p_down(float &tune_p, float tune_p_min, float tune_p_step_ratio, float target, float measurement_max);
    void autotune_updating_p_up(float &tune_p, float tune_p_max, float tune_p_step_ratio, float target, float measurement_max);
    void autotune_updating_p_up_d_down(float &tune_d, float tune_d_min, float tune_d_step_ratio, float &tune_p, float tune_p_min, float tune_p_max, float tune_p_step_ratio, float target, float measurement_min, float measurement_max);
    void autotune_twitching_measure_acceleration(float &rate_of_change, float rate_measurement, float &rate_measurement_max);

};
#endif


class FlightController_BRAKE : public FlightController {

public:

    FlightController_BRAKE(Copter &copter) :
        Copter::FlightController(copter)
        { }

    bool init(bool ignore_checks) override;
    void run() override; // should be called at 100hz or more

    bool requires_GPS() const override { return true; }
    bool has_manual_throttle() const override { return false; }
    bool allows_arming(bool from_gcs) const override { return false; };
    bool is_autopilot() const override { return false; }

protected:

    const char *name() const override { return "BRAKE"; }

private:

};


class FlightController_CIRCLE : public FlightController {

public:

    FlightController_CIRCLE(Copter &copter) :
        Copter::FlightController(copter)
        { }

    bool init(bool ignore_checks) override;
    void run() override; // should be called at 100hz or more

    bool requires_GPS() const override { return true; }
    bool has_manual_throttle() const override { return false; }
    bool allows_arming(bool from_gcs) const override { return false; };
    bool is_autopilot() const override { return false; }

protected:

    const char *name() const override { return "CIRCLE"; }

private:

    // Circle
    bool pilot_yaw_override = false; // true if pilot is overriding yaw

};


class FlightController_DRIFT : public FlightController {

public:

    FlightController_DRIFT(Copter &copter) :
        Copter::FlightController(copter)
        { }

    virtual bool init(bool ignore_checks) override;
    virtual void run() override; // should be called at 100hz or more

    virtual bool requires_GPS() const override { return true; }
    virtual bool has_manual_throttle() const override { return false; }
    virtual bool allows_arming(bool from_gcs) const override { return true; };
    virtual bool is_autopilot() const override { return false; }

protected:

    const char *name() const override { return "DRIFT"; }

private:

    float get_throttle_assist(float velz, float pilot_throttle_scaled);

};


class FlightController_FLIP : public FlightController {

public:

    FlightController_FLIP(Copter &copter) :
        Copter::FlightController(copter)
        { }

    virtual bool init(bool ignore_checks) override;
    virtual void run() override; // should be called at 100hz or more

    virtual bool requires_GPS() const override { return false; }
    virtual bool has_manual_throttle() const override { return false; }
    virtual bool allows_arming(bool from_gcs) const override { return false; };
    virtual bool is_autopilot() const override { return false; }

protected:

    const char *name() const override { return "FLIP"; }

private:

    // Flip
    Vector3f flip_orig_attitude;         // original copter attitude before flip

};


class FlightController_GUIDED : public FlightController {

public:

    FlightController_GUIDED(Copter &copter) :
        Copter::FlightController(copter)        { }

    bool init(bool ignore_checks) override;
    void run() override; // should be called at 100hz or more

    bool requires_GPS() const override { return true; }
    bool has_manual_throttle() const override { return false; }
    bool allows_arming(bool from_gcs) const override {
        if (from_gcs) {
            return true;
        }
        return false;
    };
    bool is_autopilot() const override { return false; }

    void set_angle(const Quaternion &q, float climb_rate_cms);
    void set_destination_posvel(const Vector3f& destination, const Vector3f& velocity);
    void set_velocity(const Vector3f& velocity);
    void set_destination(const Vector3f& destination);
    bool set_destination(const Location_Class& destination);

    void limit_clear();
    void limit_init_time_and_pos();
    void limit_set(uint32_t timeout_ms, float alt_min_cm, float alt_max_cm, float horiz_max_cm);
    bool limit_check();

    bool takeoff_start(float final_alt_above_home);

    GuidedMode mode() { return guided_mode; }

protected:

    const char *name() const override { return "GUIDED"; }

private:

    void pos_control_start();
    void vel_control_start();
    void posvel_control_start();
    void angle_control_start();
    void takeoff_run();
    void pos_control_run();
    void vel_control_run();
    void posvel_control_run();
    void angle_control_run();

    // controls which controller is run (pos or vel):
    GuidedMode guided_mode = Guided_TakeOff;

};


class FlightController_LAND : public FlightController {

public:

    FlightController_LAND(Copter &copter) :
        Copter::FlightController(copter)
        { }

    bool init(bool ignore_checks) override;
    void run() override; // should be called at 100hz or more

    bool requires_GPS() const override { return false; }
    bool has_manual_throttle() const override { return false; }
    bool allows_arming(bool from_gcs) const override { return false; };
    bool is_autopilot() const override { return true; }

    float get_land_descent_speed();
    bool landing_with_GPS();
    void do_not_use_GPS();

protected:

    const char *name() const override { return "LAND"; }

private:

    void gps_run();
    void nogps_run();
};


class FlightController_LOITER : public FlightController {

public:

    FlightController_LOITER(Copter &copter) :
        Copter::FlightController(copter)
        { }

    bool init(bool ignore_checks) override;
    void run() override; // should be called at 100hz or more

    bool requires_GPS() const override { return true; }
    bool has_manual_throttle() const override { return false; }
    bool allows_arming(bool from_gcs) const override { return true; };
    bool is_autopilot() const override { return false; }

protected:

    const char *name() const override { return "LOITER"; }

private:

};


#if POSHOLD_ENABLED == ENABLED
class FlightController_POSHOLD : public FlightController {

public:

    FlightController_POSHOLD(Copter &copter) :
        Copter::FlightController(copter)
        { }

    bool init(bool ignore_checks) override;
    void run() override; // should be called at 100hz or more

    bool requires_GPS() const override { return true; }
    bool has_manual_throttle() const override { return false; }
    bool allows_arming(bool from_gcs) const override { return true; };
    bool is_autopilot() const override { return false; }

protected:

    const char *name() const override { return "POSHOLD"; }

private:

    void poshold_update_pilot_lean_angle(float &lean_angle_filtered, float &lean_angle_raw);
    int16_t poshold_mix_controls(float mix_ratio, int16_t first_control, int16_t second_control);
    void poshold_update_brake_angle_from_velocity(int16_t &brake_angle, float velocity);
    void poshold_update_wind_comp_estimate();
    void poshold_get_wind_comp_lean_angles(int16_t &roll_angle, int16_t &pitch_angle);
    void poshold_roll_controller_to_pilot_override();
    void poshold_pitch_controller_to_pilot_override();

};
#endif

class FlightController_RTL : public FlightController {

public:

    FlightController_RTL(Copter &copter) :
        Copter::FlightController(copter)
        { }

    bool init(bool ignore_checks) override;
    void run() override; // should be called at 100hz or more

    bool requires_GPS() const override { return true; }
    bool has_manual_throttle() const override { return false; }
    bool allows_arming(bool from_gcs) const override { return true; };
    bool is_autopilot() const override { return false; }

    RTLState state() { return _state; }

    void restart_without_terrain();

    // this should probably not be exposed
    bool state_complete() { return _state_complete; }

    bool landing_gear_should_be_deployed();

protected:

    const char *name() const override { return "RTL"; }

private:

    void climb_start();
    void return_start();
    void climb_return_run();
    void loiterathome_start();
    void loiterathome_run();
    void descent_start();
    void descent_run();
    void land_start();
    void land_run();
    void build_path(bool terrain_following_allowed);
    void compute_return_alt(const Location_Class &rtl_origin_point, Location_Class &rtl_return_target, bool terrain_following_allowed);

    // RTL
    RTLState _state = RTL_InitialClimb;  // records state of rtl (initial climb, returning home, etc)
    bool _state_complete = false; // set to true if the current state is completed

    struct {
        // NEU w/ origin-relative altitude
        Location_Class origin_point;
        Location_Class climb_target;
        Location_Class return_target;
        Location_Class descent_target;
        bool land;
        bool terrain_used;
    } rtl_path;

    // Loiter timer - Records how long we have been in loiter
    uint32_t _loiter_start_time = 0;
};


class FlightController_SPORT : public FlightController {

public:

    FlightController_SPORT(Copter &copter) :
        Copter::FlightController(copter)
        { }

    virtual bool init(bool ignore_checks) override;
    virtual void run() override; // should be called at 100hz or more

    virtual bool requires_GPS() const override { return false; }
    virtual bool has_manual_throttle() const override { return false; }
    virtual bool allows_arming(bool from_gcs) const override { return true; };
    virtual bool is_autopilot() const override { return false; }

protected:

    const char *name() const override { return "SPORT"; }

private:

};


class FlightController_STABILIZE : public FlightController {

public:

    FlightController_STABILIZE(Copter &copter) :
        Copter::FlightController(copter)
        { }

    virtual bool init(bool ignore_checks) override;
    virtual void run() override; // should be called at 100hz or more

    virtual bool requires_GPS() const override { return false; }
    virtual bool has_manual_throttle() const override { return true; }
    virtual bool allows_arming(bool from_gcs) const override { return true; };
    virtual bool is_autopilot() const override { return false; }

protected:

    const char *name() const override { return "STABILIZE"; }

private:

};


#if FRAME_CONFIG == HELI_FRAME
class FlightController_STABILIZE_Heli : public FlightController_STABILIZE {

public:

    FlightController_STABILIZE_Heli(Copter &copter) :
        Copter::FlightController_STABILIZE(copter)
        { }

    bool init(bool ignore_checks) override;
    void run() override; // should be called at 100hz or more

protected:

private:

};
#endif


class FlightController_THROW : public FlightController {

public:

    FlightController_THROW(Copter &copter) :
        Copter::FlightController(copter)
        { }

    bool init(bool ignore_checks) override;
    void run() override; // should be called at 100hz or more

    bool requires_GPS() const override { return true; }
    bool has_manual_throttle() const override { return false; }
    bool allows_arming(bool from_gcs) const override { return true; };
    bool is_autopilot() const override { return false; }

    void throw_exit();
    bool throw_early_exit_interlock = true; // value of the throttle interlock that must be restored when exiting throw mode early

protected:

    const char *name() const override { return "THROW"; }

private:

    // Throw
    bool throw_flight_commenced = false;    // true when the throw has been detected and the motors and control loops are running
    uint32_t throw_free_fall_start_ms = 0;  // system time free fall was detected
    float throw_free_fall_start_velz = 0.0f;// vertical velocity when free fall was detected

    // Throw to launch functionality
    bool throw_detected();
    bool throw_attitude_good();
    bool throw_height_good();

};
