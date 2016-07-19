#include "Copter.h"
#include "avoidance.h"
#include <AP_Notify/AP_Notify.h>

void Copter::avoidance_update(void)
{
    avoidance.update();
}

#include <stdio.h>

AvoidanceHandler &AP_Avoidance_Copter::handler_for_action(MAV_COLLISION_ACTION action)
{
    if (copter.ap.land_complete) {
        // do nothing if we are not flying
        if (action == MAV_COLLISION_ACTION_REPORT) {
            return avoidance_handler_report;
        }
        return avoidance_handler_none;
    }

    switch(action) {
    case MAV_COLLISION_ACTION_NONE:
        return avoidance_handler_none;
    case MAV_COLLISION_ACTION_REPORT:
        return avoidance_handler_report;
    case MAV_COLLISION_ACTION_RTL:
        return avoidance_handler_rtl;
    case MAV_COLLISION_ACTION_HOVER:
        return avoidance_handler_hover;
    case MAV_COLLISION_ACTION_TCAS:
        return avoidance_handler_tcas;
    case MAV_COLLISION_ACTION_MOVE_PERPENDICULAR:
        return avoidance_handler_perpendicular;
    default:
        ::fprintf(stderr, "Avoidance action %d not known\n", action);
        internal_error();
    };
    return avoidance_handler_rtl;
}
