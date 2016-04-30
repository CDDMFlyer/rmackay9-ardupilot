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

    void get_pilot_desired_yaw_rate(int16_t yaw_in, float &yaw_out);

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
    void takeoff_start(float final_alt_above_home);
    void wp_start(const Vector3f& destination);
    void land_start();
    void land_start(const Vector3f& destination);
    void circle_movetoedge_start();
    void circle_start();
    void spline_start(const Vector3f& destination, bool stopped_at_start, AC_WPNav::spline_segment_end_type seg_end_type, const Vector3f& next_spline_destination);
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

    void limit_clear();
    void limit_init_time_and_pos();
    void limit_set(uint32_t timeout_ms, float alt_min_cm, float alt_max_cm, float horiz_max_cm);
    bool limit_check();

    void takeoff_start(float final_alt_above_home);

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
    void build_path();
    float compute_return_alt_above_origin(float rtl_return_dist);

    // RTL
    RTLState _state = RTL_InitialClimb;  // records state of rtl (initial climb, returning home, etc)
    bool _state_complete = false; // set to true if the current state is completed

    struct {
        // NEU w/ origin-relative altitude
        Vector3f origin_point;
        Vector3f climb_target;
        Vector3f return_target;
        Vector3f descent_target;
        bool land;
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
