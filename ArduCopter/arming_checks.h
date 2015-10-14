// -*- tab-width: 4; Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*-
/*
  additional arming checks for copter
 */

#ifndef __ARMING_CHECKS_H__
#define __ARMING_CHECKS_H__

#include <AP_Arming/AP_Arming.h>
#include <RC_Channel/RC_Channel.h>

/*
  copter specific arming class
 */

class AP_Arming_Copter : public AP_Arming
{
public:
    AP_Arming_Copter(const AP_AHRS &ahrs_ref, const AP_Baro &baro, Compass &compass, const enum HomeState &home_state,
            RC_Channel* channel_roll, RC_Channel* channel_pitch, RC_Channel* channel_throttle, RC_Channel* channel_yaw) :
        AP_Arming(ahrs_ref, baro, compass, home_state),
        _channel_roll(channel_roll),
        _channel_pitch(channel_pitch),
        _channel_throttle(channel_throttle),
        _channel_yaw(channel_yaw)
    {
            AP_Param::setup_object_defaults(this, var_info);
    }

    bool pre_arm_checks(bool report);

    bool manual_transmitter_checks(bool report);

    // var_info for holding Parameter information
    static const struct AP_Param::GroupInfo var_info[];

private:
    RC_Channel* _channel_roll;
    RC_Channel* _channel_pitch;
    RC_Channel* _channel_throttle;
    RC_Channel* _channel_yaw;
};

#endif //  __ARMING_CHECKS_H__
