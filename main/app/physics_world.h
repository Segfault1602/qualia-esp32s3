#pragma once

#include <cstdint>

#include <box2d/box2d.h>

namespace physics
{
constexpr std::uint32_t STEPS_PER_SECOND = 60;
constexpr int SUB_STEPS = 4;
// Gravity with the screen upright; the IMU tilt replaces it at runtime.
constexpr b2Vec2 DEFAULT_GRAVITY{0.0f, -10.0f};
constexpr float GRAVITY_MAGNITUDE = 10.0f;
constexpr b2Vec2 BORDER_CENTER{0.5f, 0.5f};
// Inner edge of the ring drawn just inside the round panel.
constexpr float BORDER_RADIUS = 234.0f / 480.0f;
// Also sets the drawn ball size (41 px) in ball.cpp.
constexpr float BALL_RADIUS = 20.5f / 480.0f;

b2WorldId create_world();
// Closed circular wall that keeps the ball inside the round display.
b2BodyId create_border(b2WorldId world);
b2BodyId create_ball(b2WorldId world, b2Vec2 position);
void step(b2WorldId world);
}
