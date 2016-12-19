#include "AC_Avoid.h"

const AP_Param::GroupInfo AC_Avoid::var_info[] = {

    // @Param: ENABLE
    // @DisplayName: Avoidance control enable/disable
    // @Description: Enabled/disable stopping at fence
    // @Values: 0:None,1:StopAtFence,2:UseProximitySensor,3:All
    // @Bitmask: 0:StopAtFence,1:UseProximitySensor
    // @User: Standard
    AP_GROUPINFO("ENABLE", 1,  AC_Avoid, _enabled, AC_AVOID_ALL),

    // @Param: ANGLE MAX
    // @DisplayName: Avoidance max lean angle in non-GPS flight modes
    // @Description: Max lean angle used to avoid obstacles while in non-GPS modes
    // @Range: 0 4500
    // @User: Standard
    AP_GROUPINFO("ANGLE_MAX", 2,  AC_Avoid, _angle_max, 1000),

    // @Param: NOGPS_P
    // @DisplayName: Avoidance P gain for non-GPS flight modes
    // @Description: Avoidance P gain for non-GPS flight modes
    // @Range: 0 5
    // @Increment: 0.1
    // @User: Advanced

    // @Param: NOGPS_I
    // @DisplayName: Avoidance I gain for non-GPS flight modes
    // @Description: Avoidance I gain for non-GPS flight modes
    // @Range: 0 5
    // @Increment: 0.1
    // @User: Advanced

    // @Param: NOGPS_IMAX
    // @DisplayName: Avoidance I gain output maximum for non-GPS flight modes
    // @Description: Avoidance I gain output maximum  for non-GPS flight modes
    // @Range: 0 1
    // @Increment: 0.1
    // @User: Advanced

    // @Param: NOGPS_FILT
    // @DisplayName: Avoidance gain for non-GPS flight modes
    // @Description: Avoidance gain for non-GPS flight modes
    // @Units: hz
    // @Increment: 0.11
    // @User: Advanced
    AP_SUBGROUPINFO(_nongps_pid, "NOGPS_", 3, AC_Avoid, AC_PI_2D),

    AP_GROUPEND
};

/// Constructor
AC_Avoid::AC_Avoid(const AP_AHRS& ahrs, const AP_InertialNav& inav, const AC_Fence& fence, const AP_Proximity& proximity)
    : _ahrs(ahrs),
      _inav(inav),
      _fence(fence),
      _proximity(proximity),
      _nongps_pid(AC_AVOID_NONGPS_P, AC_AVOID_NONGPS_I, AC_AVOID_NONGPS_IMAX, AC_AVOID_NONGPS_FILT_HZ, AC_AVOID_NONGPS_DT)
{
    AP_Param::setup_object_defaults(this, var_info);
}

void AC_Avoid::adjust_velocity(float kP, float accel_cmss, Vector2f &desired_vel)
{
    // exit immediately if disabled
    if (_enabled == AC_AVOID_DISABLED) {
        return;
    }

    // limit acceleration
    float accel_cmss_limited = MIN(accel_cmss, AC_AVOID_ACCEL_CMSS_MAX);

    if ((_enabled & AC_AVOID_STOP_AT_FENCE) > 0) {
        adjust_velocity_circle_fence(kP, accel_cmss_limited, desired_vel);
        adjust_velocity_polygon_fence(kP, accel_cmss_limited, desired_vel);
    }

    if ((_enabled & AC_AVOID_USE_PROXIMITY_SENSOR) > 0) {
        adjust_velocity_proximity(kP, accel_cmss_limited, desired_vel);
    }
}

// convenience function to accept Vector3f.  Only x and y are adjusted
void AC_Avoid::adjust_velocity(float kP, float accel_cmss, Vector3f &desired_vel)
{
    Vector2f des_vel_xy(desired_vel.x, desired_vel.y);
    adjust_velocity(kP, accel_cmss, des_vel_xy);
    desired_vel.x = des_vel_xy.x;
    desired_vel.y = des_vel_xy.y;
}

// adjust roll-pitch to push vehicle away from objects
// roll and pitch value are in centi-degrees
void AC_Avoid::adjust_roll_pitch(float &roll, float &pitch, float angle_max)
{
    // exit immediately if angle max is zero
    if (_angle_max <= 0.0f || angle_max <= 0.0f) {
        return;
    }

    float roll_force_pos = 0.0f;    // maximum positive roll force
    float roll_force_neg = 0.0f;    // minimum negative roll force
    float pitch_force_pos = 0.0f;   // maximum position pitch force
    float pitch_force_neg = 0.0f;   // minimum negative pitch force

    // get maximum positive and negative roll and pitch forces from all sources
    get_proximity_roll_pitch_force(roll_force_pos, roll_force_neg, pitch_force_pos, pitch_force_neg);

    // create final vector
    Vector2f rp_force(roll_force_pos + roll_force_neg, pitch_force_pos + pitch_force_neg);

    // pass through 2D PID controller to convert to lean angle
    _nongps_pid.set_input(rp_force);

    // get p
    Vector2f rp_out = _nongps_pid.get_p();

    // add i term to output (note: we update i term if we have not hit the angular limits on the previous iteration)
    if (!_nongps_angle_limit) {
        rp_out += _nongps_pid.get_i();
    } else {
        rp_out += _nongps_pid.get_i_shrink();
    }

    // convert to lean angle in centi-degrees
    rp_out *= 4500.0f;

    // apply avoidance angular limits
    float vec_len = rp_out.length();
    if (vec_len > _angle_max) {
        rp_out *= (_angle_max / vec_len);
        _nongps_angle_limit = true;
    } else {
        _nongps_angle_limit = false;
    }

    // add passed in roll, pitch angles
    rp_out.x += roll;
    rp_out.y += pitch;

    // apply total angular limits
    vec_len = rp_out.length();
    if (vec_len > angle_max) {
        rp_out *= (angle_max / vec_len);
        _nongps_angle_limit = true;
    }

    // return adjusted roll, pitch
    roll = rp_out.x;
    pitch = rp_out.y;
}

/*
 * Adjusts the desired velocity for the circular fence.
 */
void AC_Avoid::adjust_velocity_circle_fence(float kP, float accel_cmss, Vector2f &desired_vel)
{
    // exit if circular fence is not enabled
    if ((_fence.get_enabled_fences() & AC_FENCE_TYPE_CIRCLE) == 0) {
        return;
    }

    // exit if the circular fence has already been breached
    if ((_fence.get_breaches() & AC_FENCE_TYPE_CIRCLE) != 0) {
        return;
    }

    // get position as a 2D offset in cm from ahrs home
    const Vector2f position_xy = get_position();

    float speed = desired_vel.length();
    // get the fence radius in cm
    const float fence_radius = _fence.get_radius() * 100.0f;
    // get the margin to the fence in cm
    const float margin = get_margin();

    if (!is_zero(speed) && position_xy.length() <= fence_radius) {
        // Currently inside circular fence
        Vector2f stopping_point = position_xy + desired_vel*(get_stopping_distance(kP, accel_cmss, speed)/speed);
        float stopping_point_length = stopping_point.length();
        if (stopping_point_length > fence_radius - margin) {
            // Unsafe desired velocity - will not be able to stop before fence breach
            // Project stopping point radially onto fence boundary
            // Adjusted velocity will point towards this projected point at a safe speed
            Vector2f target = stopping_point * ((fence_radius - margin) / stopping_point_length);
            Vector2f target_direction = target - position_xy;
            float distance_to_target = target_direction.length();
            float max_speed = get_max_speed(kP, accel_cmss, distance_to_target);
            desired_vel = target_direction * (MIN(speed,max_speed) / distance_to_target);
        }
    }
}

/*
 * Adjusts the desired velocity for the polygon fence.
 */
void AC_Avoid::adjust_velocity_polygon_fence(float kP, float accel_cmss, Vector2f &desired_vel)
{
    // exit if the polygon fence is not enabled
    if ((_fence.get_enabled_fences() & AC_FENCE_TYPE_POLYGON) == 0) {
        return;
    }

    // exit if the polygon fence has already been breached
    if ((_fence.get_breaches() & AC_FENCE_TYPE_POLYGON) != 0) {
        return;
    }

    // exit immediately if no desired velocity
    if (desired_vel.is_zero()) {
        return;
    }

    // get polygon boundary
    // Note: first point in list is the return-point (which copter does not use)
    uint16_t num_points;
    Vector2f* boundary = _fence.get_polygon_points(num_points);

    // adjust velocity using polygon
    adjust_velocity_polygon(kP, accel_cmss, desired_vel, boundary, num_points, true);
}

/*
 * Adjusts the desired velocity based on output from the proximity sensor
 */
void AC_Avoid::adjust_velocity_proximity(float kP, float accel_cmss, Vector2f &desired_vel)
{
    // exit immediately if proximity sensor is not present
    if (_proximity.get_status() != AP_Proximity::Proximity_Good) {
        return;
    }

    // exit immediately if no desired velocity
    if (desired_vel.is_zero()) {
        return;
    }

    // get boundary from proximity sensor
    uint16_t num_points;
    const Vector2f *boundary = _proximity.get_boundary_points(num_points);
    adjust_velocity_polygon(kP, accel_cmss, desired_vel, boundary, num_points, false);
}

/*
 * Adjusts the desired velocity for the polygon fence.
 */
void AC_Avoid::adjust_velocity_polygon(float kP, float accel_cmss, Vector2f &desired_vel, const Vector2f* boundary, uint16_t num_points, bool earth_frame)
{
    // exit if there are no points
    if (boundary == nullptr || num_points == 0) {
        return;
    }

    // do not adjust velocity if vehicle is outside the polygon fence
    Vector3f position;
    if (earth_frame) {
        position = _inav.get_position();
    }
    Vector2f position_xy(position.x, position.y);
    if (_fence.boundary_breached(position_xy, num_points, boundary)) {
        return;
    }

    // Safe_vel will be adjusted to remain within fence.
    // We need a separate vector in case adjustment fails,
    // e.g. if we are exactly on the boundary.
    Vector2f safe_vel(desired_vel);

    // if boundary points are in body-frame, rotate velocity vector from earth frame to body-frame
    if (!earth_frame) {
        safe_vel.x = desired_vel.y * _ahrs.sin_yaw() + desired_vel.x * _ahrs.cos_yaw(); // right
        safe_vel.y = desired_vel.y * _ahrs.cos_yaw() - desired_vel.x * _ahrs.sin_yaw(); // forward
    }

    uint16_t i, j;
    for (i = 1, j = num_points-1; i < num_points; j = i++) {
        // end points of current edge
        Vector2f start = boundary[j];
        Vector2f end = boundary[i];
        // vector from current position to closest point on current edge
        Vector2f limit_direction = Vector2f::closest_point(position_xy, start, end) - position_xy;
        // distance to closest point
        const float limit_distance = limit_direction.length();
        if (!is_zero(limit_distance)) {
            // We are strictly inside the given edge.
            // Adjust velocity to not violate this edge.
            limit_direction /= limit_distance;
            limit_velocity(kP, accel_cmss, safe_vel, limit_direction, MAX(limit_distance - get_margin(),0.0f));
        } else {
            // We are exactly on the edge - treat this as a fence breach.
            // i.e. do not adjust velocity.
            return;
        }
    }

    // set modified desired velocity vector
    if (earth_frame) {
        desired_vel = safe_vel;
    } else {
        // if points were in body-frame, rotate resulting vector back to earth-frame
        desired_vel.x = safe_vel.x * _ahrs.cos_yaw() - safe_vel.y * _ahrs.sin_yaw();
        desired_vel.y = safe_vel.x * _ahrs.sin_yaw() + safe_vel.y * _ahrs.cos_yaw();
    }
}

/*
 * Limits the component of desired_vel in the direction of the unit vector
 * limit_direction to be at most the maximum speed permitted by the limit_distance.
 *
 * Uses velocity adjustment idea from Randy's second email on this thread:
 * https://groups.google.com/forum/#!searchin/drones-discuss/obstacle/drones-discuss/QwUXz__WuqY/qo3G8iTLSJAJ
 */
void AC_Avoid::limit_velocity(float kP, float accel_cmss, Vector2f &desired_vel, const Vector2f& limit_direction, float limit_distance) const
{
    const float max_speed = get_max_speed(kP, accel_cmss, limit_distance);
    // project onto limit direction
    const float speed = desired_vel * limit_direction;
    if (speed > max_speed) {
        // subtract difference between desired speed and maximum acceptable speed
        desired_vel += limit_direction*(max_speed - speed);
    }
}

/*
 * Gets the current xy-position, relative to home (not relative to EKF origin)
 */
Vector2f AC_Avoid::get_position()
{
    const Vector3f position_xyz = _inav.get_position();
    const Vector2f position_xy(position_xyz.x,position_xyz.y);
    const Vector2f diff = location_diff(_inav.get_origin(),_ahrs.get_home()) * 100.0f;
    return position_xy - diff;
}

/*
 * Computes the speed such that the stopping distance
 * of the vehicle will be exactly the input distance.
 */
float AC_Avoid::get_max_speed(float kP, float accel_cmss, float distance) const
{
    return AC_AttitudeControl::sqrt_controller(distance, kP, accel_cmss);
}

/*
 * Computes distance required to stop, given current speed.
 *
 * Implementation copied from AC_PosControl.
 */
float AC_Avoid::get_stopping_distance(float kP, float accel_cmss, float speed) const
{
    // avoid divide by zero by using current position if the velocity is below 10cm/s, kP is very low or acceleration is zero
    if (kP <= 0.0f || accel_cmss <= 0.0f || is_zero(speed)) {
        return 0.0f;
    }

    // calculate distance within which we can stop
    // accel_cmss/kP is the point at which velocity switches from linear to sqrt
    if (speed < accel_cmss/kP) {
        return speed/kP;
    } else {
        // accel_cmss/(2.0f*kP*kP) is the distance at which we switch from linear to sqrt response
        return accel_cmss/(2.0f*kP*kP) + (speed*speed)/(2.0f*accel_cmss);
    }
}

// converts distance (in meters) to a force (in 0~1 range) for use in manual flight modes
float AC_Avoid::distance_to_force(float dist_m)
{
    if (dist_m <= 0.0f || dist_m > 10.0f) {
        return 0.0f;
    }
    if (dist_m <= 1.0f) {
        return 1.0f;
    }
    return 1.0f/dist_m;
}

// returns the maximum positive and negative roll and pitch forces based on the proximity sensor
//   all values are in the 0 ~ 1 range
void AC_Avoid::get_proximity_roll_pitch_force(float &roll_force_pos, float &roll_force_neg, float &pitch_force_pos, float &pitch_force_neg)
{
    // exit immediately if proximity sensor is not present
    if (_proximity.get_status() != AP_Proximity::Proximity_Good) {
        return;
    }

    uint8_t obj_count = _proximity.get_object_count();

    // if no objects return
    if (obj_count == 0) {
        return;
    }

    // calculate maximum roll, pitch force from objects
    for (uint8_t i=0; i<obj_count; i++) {
        float ang_deg, dist_m;
        if (_proximity.get_object_angle_and_distance(i, ang_deg, dist_m)) {
            if (dist_m < AC_AVOID_NONGPS_DIST_MAX) {
                // convert distance to force
                float force = distance_to_force(dist_m);
                // convert to angle and force to roll and pitch force
                float angle_rad = radians(ang_deg);
                float roll_force = -sinf(angle_rad) * force;
                float pitch_force = cosf(angle_rad) * force;
                // update roll, pitch force maximums
                if (roll_force > 0.0f) {
                    roll_force_pos = MAX(roll_force_pos, roll_force);
                }
                if (roll_force < 0.0f) {
                    roll_force_neg = MIN(roll_force_neg, roll_force);
                }
                if (pitch_force > 0.0f) {
                    pitch_force_pos = MAX(pitch_force_pos, pitch_force);
                }
                if (pitch_force < 0.0f) {
                    pitch_force_neg = MIN(pitch_force_neg, pitch_force);
                }
            }
        }
    }
}
