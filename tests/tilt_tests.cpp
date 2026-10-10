#include "tilt.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    std::exit(EXIT_FAILURE); \
} } while (false)

namespace
{
constexpr float G = 9.80665f;
constexpr float MAGNITUDE = 10.0f;

bool near(b2Vec2 actual, b2Vec2 expected)
{
    return std::fabs(actual.x - expected.x) < 1e-4f && std::fabs(actual.y - expected.y) < 1e-4f;
}
} // namespace

int main()
{
    // Sensor +X to the right, +Y up, +Z out of the screen.
    constexpr tilt::AxisMap aligned{{tilt::Axis::X, 1.0f}, {tilt::Axis::Y, 1.0f}};

    // Upright: specific force points up the screen, gravity points down it.
    CHECK(near(tilt::gravity_from_accel({0.0f, G, 0.0f}, aligned, MAGNITUDE), {0.0f, -MAGNITUDE}));
    // Upside down.
    CHECK(near(tilt::gravity_from_accel({0.0f, -G, 0.0f}, aligned, MAGNITUDE), {0.0f, MAGNITUDE}));
    // Rotated so the right edge points down.
    CHECK(near(tilt::gravity_from_accel({G, 0.0f, 0.0f}, aligned, MAGNITUDE), {-MAGNITUDE, 0.0f}));
    // Flat, screen up: nothing in the display plane.
    CHECK(near(tilt::gravity_from_accel({0.0f, 0.0f, G}, aligned, MAGNITUDE), {0.0f, 0.0f}));
    // Leaning back 60 degrees from upright keeps half of the in-plane pull.
    const float lean = 60.0f * B2_PI / 180.0f;
    CHECK(near(tilt::gravity_from_accel({0.0f, G * std::cos(lean), G * std::sin(lean)}, aligned, MAGNITUDE),
               {0.0f, -MAGNITUDE * 0.5f}));
    // Shaking past 1 g is clamped.
    const b2Vec2 shaken = tilt::gravity_from_accel({3.0f * G, 0.0f, 0.0f}, aligned, MAGNITUDE);
    CHECK(std::fabs(b2Length(shaken) - MAGNITUDE) < 1e-4f);

    // A sensor mounted rotated 90 degrees with its Z axis toward the display's up direction.
    constexpr tilt::AxisMap rotated{{tilt::Axis::Y, -1.0f}, {tilt::Axis::Z, 1.0f}};
    CHECK(near(tilt::gravity_from_accel({0.0f, 0.0f, G}, rotated, MAGNITUDE), {0.0f, -MAGNITUDE}));
    CHECK(near(tilt::gravity_from_accel({0.0f, -G, 0.0f}, rotated, MAGNITUDE), {-MAGNITUDE, 0.0f}));
    CHECK(near(tilt::gravity_from_accel({G, 0.0f, 0.0f}, rotated, MAGNITUDE), {0.0f, 0.0f}));

    // Readings captured on the Qualia: upright, then rotated with the right edge down.
    const b2Vec2 upright = tilt::gravity_from_accel({-0.5f, -9.84f, 1.5f}, tilt::BOARD_AXIS_MAP, MAGNITUDE);
    CHECK(upright.y < -9.5f && std::fabs(upright.x) < 1.0f);
    const b2Vec2 right_down = tilt::gravity_from_accel({9.62f, -0.45f, 1.3f}, tilt::BOARD_AXIS_MAP, MAGNITUDE);
    CHECK(right_down.x > 9.5f && std::fabs(right_down.y) < 1.0f);
    return EXIT_SUCCESS;
}
