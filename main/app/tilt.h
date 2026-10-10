#pragma once

#include <box2d/math_functions.h>

#include "sensor_types.h"

namespace tilt
{
enum class Axis
{
    X,
    Y,
    Z,
};

struct SignedAxis
{
    Axis axis;
    float sign;
};

/**
 * @brief Sensor axes that point along the display's x (right) and y (up) directions.
 */
struct AxisMap
{
    SignedAxis x;
    SignedAxis y;
};

// How the breakout is mounted behind the Qualia panel: rotated 180 degrees about Z relative to the display.
// Upright, the accelerometer reads -Y; with the right edge down, it reads +X.
constexpr AxisMap BOARD_AXIS_MAP{{Axis::X, -1.0f}, {Axis::Y, -1.0f}};

constexpr float component(const imu::Vec3& v, SignedAxis axis)
{
    switch (axis.axis)
    {
    case Axis::X:
        return axis.sign * v.x;
    case Axis::Y:
        return axis.sign * v.y;
    case Axis::Z:
        return axis.sign * v.z;
    }
    return 0.0f;
}

/**
 * @brief World gravity from an accelerometer reading.
 *
 * The accelerometer reports specific force, which points up at rest, so gravity is its negation. Only the
 * part in the display plane moves the ball: upright gives full gravity, flat gives none.
 *
 * @param accel Specific force in m/s^2, sensor frame
 * @param magnitude World gravity for a 1 g in-plane reading; also the upper bound of the result
 */
inline b2Vec2 gravity_from_accel(const imu::Vec3& accel, const AxisMap& map, float magnitude)
{
    constexpr float STANDARD_GRAVITY = 9.80665f;
    const float scale = magnitude / STANDARD_GRAVITY;
    b2Vec2 gravity{-component(accel, map.x) * scale, -component(accel, map.y) * scale};
    const float length = b2Length(gravity);
    if (length > magnitude)
    {
        gravity = b2MulSV(magnitude / length, gravity);
    }
    return gravity;
}
} // namespace tilt
