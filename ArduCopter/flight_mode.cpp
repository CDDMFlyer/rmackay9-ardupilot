/// -*- tab-width: 4; Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*-

#include "Copter.h"

/*
 * flight.pde - high level calls to set and update flight modes
 *      logic for individual flight modes is in control_acro.pde, control_stabilize.pde, etc
 */

// set_mode - change flight mode and perform any necessary initialisation
// optional force parameter used to force the flight mode change (used only first time mode is set)
// returns true if mode was succesfully set
// ACRO, STABILIZE, ALTHOLD, LAND, DRIFT and SPORT can always be set successfully but the return state of other flight modes should be checked and the caller should deal with failures appropriately
bool Copter::set_mode(control_mode_t mode, mode_reason_t reason)
{
    // boolean to record if flight mode could be set
    bool success = false;
    bool ignore_checks = !motors.armed();   // allow switching to any mode if disarmed.  We rely on the arming check to perform

    // return immediately if we are already in the desired mode
    if (mode == control_mode) {
        prev_control_mode = control_mode;
        prev_control_mode_reason = control_mode_reason;

        control_mode_reason = reason;
        return true;
    }

    // for transition, we assume no controller object will be used in
    // the new mode, and if the transition fails we reset the
    // controller to the previous value
    Copter::FlightController* old_controller = controller;
    controller = NULL;

    switch(mode) {
        case ACRO:
                success = controller_acro.init(ignore_checks);
                if (success) {
                    controller = &controller_acro;
                }
            break;

        case STABILIZE:
                success = controller_stabilize.init(ignore_checks);
                if (success) {
                    controller = &controller_stabilize;
                }
            break;

        case ALT_HOLD:
            success = controller_althold.init(ignore_checks);
            if (success) {
                controller = &controller_althold;
            }
            break;

        case AUTO:
            success = controller_auto.init(ignore_checks);
            if (success) {
                controller = &controller_auto;
            }
            break;

        case CIRCLE:
            success = controller_circle.init(ignore_checks);
            if (success) {
                controller = &controller_circle;
            }
            break;

        case LOITER:
            success = controller_loiter.init(ignore_checks);
            if (success) {
                controller = &controller_loiter;
            }
            break;

        case GUIDED:
            success = controller_guided.init(ignore_checks);
            if (success) {
                controller = &controller_guided;
            }
            break;

        case LAND:
            success = controller_land.init(ignore_checks);
            if (success) {
                controller = &controller_land;
            }
            break;

        case RTL:
            success = controller_rtl.init(ignore_checks);
            if (success) {
                controller = &controller_rtl;
            }
            break;

        case DRIFT:
            success = controller_drift.init(ignore_checks);
            if (success) {
                controller = &controller_drift;
            }
            break;

        case SPORT:
            success = sport_init(ignore_checks);
            break;

        case FLIP:
            success = flip_init(ignore_checks);
            break;

#if AUTOTUNE_ENABLED == ENABLED
        case AUTOTUNE:
            success = autotune_init(ignore_checks);
            break;
#endif

#if POSHOLD_ENABLED == ENABLED
        case POSHOLD:
            success = poshold_init(ignore_checks);
            break;
#endif

        case BRAKE:
            success = brake_init(ignore_checks);
            break;

        case THROW:
            success = throw_init(ignore_checks);
            break;

        default:
            success = false;
            break;
    }

    // update flight mode
    if (success) {
        // perform any cleanup required by previous flight mode
        exit_mode(control_mode, mode);
        
        prev_control_mode = control_mode;
        prev_control_mode_reason = control_mode_reason;

        control_mode = mode;
        control_mode_reason = reason;
        DataFlash.Log_Write_Mode(control_mode, control_mode_reason);

#if AC_FENCE == ENABLED
        // pilot requested flight mode change during a fence breach indicates pilot is attempting to manually recover
        // this flight mode change could be automatic (i.e. fence, battery, GPS or GCS failsafe)
        // but it should be harmless to disable the fence temporarily in these situations as well
        fence.manual_recovery_start();
#endif
    }else{
        controller = old_controller;
        // Log error that we failed to enter desired flight mode
        Log_Write_Error(ERROR_SUBSYSTEM_FLIGHT_MODE,mode);
    }

    // update notify object
    if (success) {
        notify_flight_mode();
    }

    // return success or failure
    return success;
}

// update_flight_mode - calls the appropriate attitude controllers based on flight mode
// called at 100hz or more
void Copter::update_flight_mode()
{
    // Update EKF speed limit - used to limit speed when we are using optical flow
    ahrs.getEkfControlLimits(ekfGndSpdLimit, ekfNavVelGainScaler);

    if (controller != NULL) {
        controller->run();
    }

    switch (control_mode) {

        case SPORT:
            sport_run();
            break;

        case FLIP:
            flip_run();
            break;

#if AUTOTUNE_ENABLED == ENABLED
        case AUTOTUNE:
            autotune_run();
            break;
#endif

#if POSHOLD_ENABLED == ENABLED
        case POSHOLD:
            poshold_run();
            break;
#endif

        case BRAKE:
            brake_run();
            break;

        case THROW:
            throw_run();
            break;

        default:
            break;
    }
}

// exit_mode - high level call to organise cleanup as a flight mode is exited
void Copter::exit_mode(control_mode_t old_control_mode, control_mode_t new_control_mode)
{
#if AUTOTUNE_ENABLED == ENABLED
    if (old_control_mode == AUTOTUNE) {
        autotune_stop();
    }
#endif

    // stop mission when we leave auto mode
    if (old_control_mode == AUTO) {
        if (mission.state() == AP_Mission::MISSION_RUNNING) {
            mission.stop();
        }
#if MOUNT == ENABLED
        camera_mount.set_mode_to_default();
#endif  // MOUNT == ENABLED
    }

    if (old_control_mode == THROW) {
        throw_exit();
    }

    // smooth throttle transition when switching from manual to automatic flight modes
    if (mode_has_manual_throttle(old_control_mode) && !mode_has_manual_throttle(new_control_mode) && motors.armed() && !ap.land_complete) {
        // this assumes all manual flight modes use get_pilot_desired_throttle to translate pilot input to output throttle
        set_accel_throttle_I_from_pilot_throttle(get_pilot_desired_throttle(channel_throttle->control_in));
    }

    // cancel any takeoffs in progress
    takeoff_stop();

#if FRAME_CONFIG == HELI_FRAME
    // firmly reset the flybar passthrough to false when exiting acro mode.
    if (old_control_mode == ACRO) {
        attitude_control.use_flybar_passthrough(false, false);
        motors.set_acro_tail(false);
    }

    // if we are changing from a mode that did not use manual throttle,
    // stab col ramp value should be pre-loaded to the correct value to avoid a twitch
    // heli_stab_col_ramp should really only be active switching between Stabilize and Acro modes
    if (!mode_has_manual_throttle(old_control_mode)){
        if (new_control_mode == STABILIZE){
            input_manager.set_stab_col_ramp(1.0);
        } else if (new_control_mode == ACRO){
            input_manager.set_stab_col_ramp(0.0);
        }
    }
#endif //HELI_FRAME
}

// returns true or false whether current control mode requires GPS
bool Copter::mode_requires_GPS() {
    if (controller != NULL) {
        return controller->requires_GPS();
    }
    switch(control_mode) {
        case POSHOLD:
        case BRAKE:
        case THROW:
            return true;
        default:
            return false;
    }

    return false;
}

// mode_has_manual_throttle - returns true if the flight mode has a manual throttle (i.e. pilot directly controls throttle)
bool Copter::mode_has_manual_throttle(control_mode_t mode) {
    switch(mode) {
        case ACRO:
        case STABILIZE:
            return true;
        default:
            return false;
    }

    return false;
}

// mode_allows_arming - returns true if vehicle can be armed in the current mode
//  arming_from_gcs should be set to true if the arming request comes from the ground station
bool Copter::mode_allows_arming(bool arming_from_gcs) {
    if (controller != NULL) {
        return controller->allows_arming(arming_from_gcs);
    }
    uint8_t mode = control_mode;
    if (mode_has_manual_throttle(mode) || mode == LOITER || mode == ALT_HOLD || mode == POSHOLD || mode == DRIFT || mode == SPORT || mode == THROW || (arming_from_gcs && mode == GUIDED)) {
        return true;
    }
    return false;
}

// notify_flight_mode - sets notify object based on current flight mode.  Only used for OreoLED notify device
void Copter::notify_flight_mode() {
    if (controller != NULL) {
        AP_Notify::flags.autopilot_mode = controller->is_autopilot();
        return;
    }
    switch(control_mode) {
    default:
            // all other are manual flight modes
            AP_Notify::flags.autopilot_mode = false;
            break;
    }
}

//
// print_flight_mode - prints flight mode to serial port.
//
void Copter::print_flight_mode(AP_HAL::BetterStream *port, uint8_t mode)
{
    if (controller != NULL) {
        controller->print_FlightMode(port);
        return;
    }
    switch (mode) {
    case SPORT:
        port->print("SPORT");
        break;
    case FLIP:
        port->print("FLIP");
        break;
    case AUTOTUNE:
        port->print("AUTOTUNE");
        break;
    case POSHOLD:
        port->print("POSHOLD");
        break;
    case BRAKE:
        port->print("BRAKE");
        break;
    case THROW:
        port->print("THROW");
        break;
    default:
        port->printf("Mode(%u)", (unsigned)mode);
        break;
    }
}

