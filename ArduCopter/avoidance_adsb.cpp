#include "Copter.h"
#include <AP_Notify/AP_Notify.h>

void Copter::avoidance_adsb_update(void)
{
    avoidance_adsb.update();
}

#include <stdio.h>

MAV_COLLISION_ACTION AP_Avoidance_Copter::handle_avoidance(const AP_Avoidance::Obstacle *obstacle, MAV_COLLISION_ACTION requested_action)
{
    // take no action if disarmed or landedok
    if (!copter.motors.armed() || copter.ap.land_complete) {
        return MAV_COLLISION_ACTION_NONE;
    }

    // take no action in some flight modes
    if (copter.control_mode == LAND ||
        copter.control_mode == THROW ||
        copter.control_mode == FLIP) {
        return MAV_COLLISION_ACTION_NONE;
    }

    // take action based on requested action
    switch (requested_action) {

        case MAV_COLLISION_ACTION_RTL:
            if (copter.set_mode(RTL, MODE_REASON_AVOIDANCE)) {
                return MAV_COLLISION_ACTION_RTL;
            }
            break;

        case MAV_COLLISION_ACTION_HOVER:
            if (copter.set_mode(LOITER, MODE_REASON_AVOIDANCE)) {
                return MAV_COLLISION_ACTION_HOVER;
            }
            break;

        case MAV_COLLISION_ACTION_TCAS:
            // update target
            if (handle_avoidance_tcas(obstacle)) {
                return MAV_COLLISION_ACTION_TCAS;
            }
            break;

        case MAV_COLLISION_ACTION_MOVE_PERPENDICULAR:
            if (handle_avoidance_perpendicular(obstacle)) {
                return MAV_COLLISION_ACTION_MOVE_PERPENDICULAR;
            }
            break;

        // unsupported actions and those that require no response
        case MAV_COLLISION_ACTION_NONE:
        case MAV_COLLISION_ACTION_REPORT:
        default:
            break;
    }

    // if we got this far we failed to take any action
    return MAV_COLLISION_ACTION_NONE;
}

bool AP_Avoidance_Copter::handle_avoidance_perpendicular(const AP_Avoidance::Obstacle *obstacle)
{
    // ensure copter is in avoid_adsb mode
    if (copter.control_mode != AVOID_ADSB) {
        if (!copter.set_mode(AVOID_ADSB, MODE_REASON_AVOIDANCE)) {
            // failed to set mode so exit immediately
            return false;
        }
    }

    // update new target
    Vector3f dest_neu;
    if (get_destination_perpendicular(obstacle, dest_neu, copter.wp_nav.get_speed_xy(), copter.wp_nav.get_speed_up(), _minimum_avoid_height)) {
        set_avoid_adsb_destination(dest_neu);
        return true;
    }

    // if we got this far we failed to set the new target
    return false;
}

bool AP_Avoidance_Copter::handle_avoidance_tcas(const AP_Avoidance::Obstacle *obstacle)
{
    // ensure copter is in avoid_adsb mode
    if (copter.control_mode != AVOID_ADSB) {
        if (!copter.set_mode(AVOID_ADSB, MODE_REASON_AVOIDANCE)) {
            // failed to set mode so exit immediately
            return false;
        }
    }

    // get new target destination based on tcas algorithm
    int32_t target_alt_m;
    if (tcas_get_target_alt(obstacle, target_alt_m)) {
        copter.avoid_adsb_set_target_alt(target_alt_m * 100.0f);
        return true;
    }

    // if we got this far we failed to set the new target
    return false;
}

uint32_t AP_Avoidance_Copter::my_src_id(const MAV_COLLISION_SRC src) const
{
    switch (src) {
    case MAV_COLLISION_SRC_ADSB:
        // if we are actively broadcasting ADSB then we should have an ID.  Return that here
        return 0; // should we return MAX_UINT32/2 here?
    case MAV_COLLISION_SRC_MAVLINK_GPS_GLOBAL_INT:
        return mavlink_system.sysid;
    case MAV_COLLISION_SRC_ENUM_END:
        break;
    }
    return 0;
}

AP_Avoidance_Copter::tcas_resolution_t AP_Avoidance_Copter::tcas_get_resolution(const AP_Avoidance::Obstacle *obstacle)
{
    if (obstacle == nullptr) {
        // invalid object so default to descending
        return tcas_resolution_descend;
    }

    Location my_loc;
    if (!_ahrs.get_position(my_loc)) {
        // descend if we don't know our position (we should never get here)
        return tcas_resolution_descend;
    }

    // ::fprintf(stderr, "heights: me=%d threat=%d\n", my_alt, threat_alt);

    if (labs(my_loc.alt - obstacle->_location.alt) <= 100) {
       // the aircraft are within 1m of each other.  Treat this as equal
       // vehicle with higher adsb id climbs
       // Note that if these are coming from different sources (ADSB vs MAVLink sysid)
       // then this comparison doesn't make a great deal of sense.
       if (obstacle->src_id < my_src_id(obstacle->src)) {
           return tcas_resolution_ascend;
       } else if (obstacle->src_id > my_src_id(obstacle->src)) {
           return tcas_resolution_descend;
       }
       // We have the same src id as the threat.
       // Flip a coin as to whether to go up or down.
       return ((my_loc.alt % 2 == 0) ? tcas_resolution_descend : tcas_resolution_ascend);
    }

    // if higher than obstacle climb
    if (my_loc.alt > obstacle->_location.alt) {
        return tcas_resolution_ascend;
    }

    // if lower then descend
    return tcas_resolution_descend;
}

// get target altitude to execute TCAS style avoidance
// ideas adopted from: http://wiki.paparazziuav.org/wiki/MultiUAV
bool AP_Avoidance_Copter::tcas_get_target_alt(const AP_Avoidance::Obstacle *obstacle, int32_t &target_alt)
{
    // decide on whether we should climb or descend
    tcas_resolution_t rr = tcas_get_resolution(obstacle);

    // get current target altitude.  Note: vector is NEU offset from EKF origin
    int32_t delta_m = 10.0f;
    if (rr == tcas_resolution_descend) {
        delta_m = -delta_m;
    }

    // ::fprintf(stderr, "tcas_resolution=%d delta_cm=%f\n", rr, delta_cm);

    // calculate new target altitude
    Vector3f my_pos;
    if (_ahrs.get_relative_position_NED(my_pos)) {
        target_alt = -my_pos[2] + delta_m;
        return true;
    } else {
        // we don't know our current height, return failure
        return false;
    }
}

// send new destination to avoid_adsb mode's controller (and throttle updates if necessary)
void AP_Avoidance_Copter::set_avoid_adsb_destination(const Vector3f &dest)
{
    // only update our destination once per second
    uint32_t now = AP_HAL::millis();
    if (now - _last_wp_update > 1000) {
        _last_wp_update = now;
        copter.avoid_adsb_set_destination(dest);
    }
}
