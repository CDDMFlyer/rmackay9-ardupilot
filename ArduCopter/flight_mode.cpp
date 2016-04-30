/// -*- tab-width: 4; Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*-

#include "Copter.h"

/*
 * flight.pde - high level calls to set and update flight modes
 *      logic for individual flight modes is in control_acro.pde, control_stabilize.pde, etc
 */

// return the static controller object corresponding to supplied mode
Copter::FlightController *Copter::controller_for_mode(const uint8_t mode)
{
    Copter::FlightController *ret = NULL;

    switch(mode) {
        case ACRO:
            ret = &controller_acro;
            break;

        case STABILIZE:
            ret = &controller_stabilize;
            break;

        case ALT_HOLD:
            ret = &controller_althold;
            break;

        case AUTO:
            ret = &controller_auto;
            break;

        case CIRCLE:
            ret = &controller_circle;
            break;

        case LOITER:
            ret = &controller_loiter;
            break;

        case GUIDED:
            ret = &controller_guided;
            break;

        case LAND:
            ret = &controller_land;
            break;

        case RTL:
            ret = &controller_rtl;
            break;

        case DRIFT:
            ret = &controller_drift;
            break;

        case SPORT:
            ret = &controller_sport;
            break;

        case FLIP:
            ret = &controller_flip;
            break;

#if AUTOTUNE_ENABLED == ENABLED
        case AUTOTUNE:
            ret = &controller_autotune;
            break;
#endif

#if POSHOLD_ENABLED == ENABLED
        case POSHOLD:
            ret = &controller_poshold;
            break;
#endif

        case BRAKE:
            ret = &controller_brake;
            break;

        case THROW:
            ret = &controller_throw;
            break;

        default:
            break;
    }

    return ret;
}


// set_mode - change flight mode and perform any necessary initialisation
// optional force parameter used to force the flight mode change (used only first time mode is set)
// returns true if mode was succesfully set
// ACRO, STABILIZE, ALTHOLD, LAND, DRIFT and SPORT can always be set successfully but the return state of other flight modes should be checked and the caller should deal with failures appropriately
bool Copter::set_mode(control_mode_t mode, mode_reason_t reason)
{

    // return immediately if we are already in the desired mode
    if (mode == control_mode) {
        control_mode_reason = reason;
        return true;
    }

    Copter::FlightController *new_controller = controller_for_mode(mode);
    if (new_controller == NULL) {
        Log_Write_Error(ERROR_SUBSYSTEM_FLIGHT_MODE,mode);
        return false;
    }

    bool ignore_checks = !motors.armed();   // allow switching to any mode if disarmed.  We rely on the arming check to perform

    if (! new_controller->init(ignore_checks)) {
        Log_Write_Error(ERROR_SUBSYSTEM_FLIGHT_MODE,mode);
        return false;
    }

    // perform any cleanup required by previous flight mode
    exit_mode(control_mode, mode);

    // update flight mode
    controller = new_controller;
    control_mode = mode;
    control_mode_reason = reason;
    DataFlash.Log_Write_Mode(control_mode);

#if AC_FENCE == ENABLED
        // pilot requested flight mode change during a fence breach indicates pilot is attempting to manually recover
        // this flight mode change could be automatic (i.e. fence, battery, GPS or GCS failsafe)
        // but it should be harmless to disable the fence temporarily in these situations as well
    fence.manual_recovery_start();
#endif

    // update notify object
    notify_flight_mode();

    // return success
    return true;
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
    }
}

// exit_mode - high level call to organise cleanup as a flight mode is exited
void Copter::exit_mode(control_mode_t old_control_mode, control_mode_t new_control_mode)
{
#if AUTOTUNE_ENABLED == ENABLED
    if (old_control_mode == AUTOTUNE) {
        controller_autotune.autotune_stop();
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
        controller_throw.throw_exit();
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
    default:
        port->printf("Mode(%u)", (unsigned)mode);
        break;
    }
}

