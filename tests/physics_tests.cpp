#include "physics_world.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <utility>

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                                       \
            std::exit(EXIT_FAILURE);                                                                                   \
        }                                                                                                              \
    } while (false)

int main()
{
    const b2ContactEvents empty_events{};
    CHECK(!physics::strongest_bounce(empty_events, b2_nullBodyId, b2_nullBodyId));
    const b2WorldId world = physics::create_world();
    CHECK(b2World_IsValid(world));
    const b2BodyId border = physics::create_border(world);
    CHECK(b2Body_IsValid(border));
    const b2Vec2 start{0.3f, 0.8f};
    const b2BodyId ball = physics::create_ball(world, start, true);
    CHECK(b2Body_IsValid(ball));

    constexpr float contact_distance = physics::BORDER_RADIUS - physics::BALL_RADIUS;
    constexpr int steps_per_window = 5 * static_cast<int>(physics::STEPS_PER_SECOND);
    float max_distance = 0.0f;
    int first_contact_step = -1;
    int center_crossings = 0;
    bool left_of_center = start.x < physics::BORDER_CENTER.x;
    float first_window_swing = 0.0f;
    float last_window_swing = 0.0f;
    int bounce_count = 0;
    int resting_bounce_count = 0;
    bool checked_events = false;
    for (int i = 0; i < 12 * steps_per_window; ++i)
    {
        physics::step(world);
        const auto events = b2World_GetContactEvents(world);
        const auto bounce = physics::strongest_bounce(events, ball, border);
        if (bounce)
        {
            ++bounce_count;
            CHECK(bounce->approach_speed > 0.2f);
            CHECK(b2Distance(bounce->point, physics::BORDER_CENTER) > contact_distance - 0.01f);
            if (i >= 11 * steps_per_window)
            {
                ++resting_bounce_count;
            }
            if (!checked_events)
            {
                std::array<b2ContactHitEvent, 2> duplicate{events.hitEvents[0], events.hitEvents[0]};
                std::swap(duplicate[1].shapeIdA, duplicate[1].shapeIdB);
                duplicate[1].approachSpeed = duplicate[0].approachSpeed + 1.0f;
                b2ContactEvents synthetic{};
                synthetic.hitEvents = duplicate.data();
                synthetic.hitCount = 2;
                const auto strongest = physics::strongest_bounce(synthetic, ball, border);
                CHECK(strongest && strongest->approach_speed == duplicate[1].approachSpeed);
                CHECK(!physics::strongest_bounce(synthetic, ball, ball));
                duplicate[1].approachSpeed = duplicate[0].approachSpeed;
                duplicate[1].point.x += 0.01f;
                const auto tied = physics::strongest_bounce(synthetic, ball, border);
                CHECK(tied && tied->point.x == duplicate[0].point.x);
                checked_events = true;
            }
        }
        const b2Vec2 position = b2Body_GetPosition(ball);
        const float distance = b2Distance(position, physics::BORDER_CENTER);
        max_distance = std::fmax(max_distance, distance);
        if (first_contact_step < 0 && distance > contact_distance - 0.005f)
        {
            first_contact_step = i;
        }
        const bool left = position.x < physics::BORDER_CENTER.x;
        if (left != left_of_center)
        {
            ++center_crossings;
            left_of_center = left;
        }
        const float swing = std::fabs(position.x - physics::BORDER_CENTER.x);
        if (i < steps_per_window)
        {
            first_window_swing = std::fmax(first_window_swing, swing);
        }
        else if (i >= 11 * steps_per_window)
        {
            last_window_swing = std::fmax(last_window_swing, swing);
        }
    }

    std::printf("first contact step %d, max distance %.4f (contact %.4f), crossings %d, swing %.4f -> %.4f\n",
                first_contact_step, max_distance, contact_distance, center_crossings, first_window_swing,
                last_window_swing);

    // Free fall from the start until the ball meets the curved wall takes ~0.37 s.
    CHECK(first_contact_step >= 18 && first_contact_step <= 26);
    // The ball never leaves the ring beyond Box2D's contact tolerance.
    CHECK(max_distance < contact_distance + 0.01f);
    // It rolls back and forth across the lowest point, losing energy over time.
    CHECK(center_crossings >= 4);
    CHECK(last_window_swing < 0.5f * first_window_swing);
    CHECK(checked_events);
    CHECK(bounce_count >= 2);
    CHECK(resting_bounce_count == 0);
    // Rolling resistance and damping bring it to rest at the lowest point.
    const b2Vec2 rest = b2Body_GetPosition(ball);
    CHECK(b2Length(b2Body_GetLinearVelocity(ball)) < 0.003f);
    CHECK(std::fabs(rest.x - physics::BORDER_CENTER.x) < 0.01f);
    CHECK(rest.y < physics::BORDER_CENTER.y - contact_distance + 0.01f);

    // Tilting the device after the ball came to rest still moves it: here gravity turns to the right.
    b2World_SetGravity(world, {physics::GRAVITY_MAGNITUDE, 0.0f});
    for (int i = 0; i < 10 * static_cast<int>(physics::STEPS_PER_SECOND); ++i)
    {
        physics::step(world);
    }
    const b2Vec2 tilted = b2Body_GetPosition(ball);
    std::printf("after tilting right: %.4f, %.4f\n", tilted.x, tilted.y);
    CHECK(tilted.x > physics::BORDER_CENTER.x + 0.9f * contact_distance);
    CHECK(std::fabs(tilted.y - physics::BORDER_CENTER.y) < 0.05f);

    // With the screen flat, a rolling ball slows to a stop instead of coasting.
    b2World_SetGravity(world, {0.0f, 0.0f});
    b2Body_SetTransform(ball, physics::BORDER_CENTER, b2Rot_identity);
    b2Body_SetLinearVelocity(ball, {0.05f, 0.0f});
    for (int i = 0; i < 10 * static_cast<int>(physics::STEPS_PER_SECOND); ++i)
    {
        physics::step(world);
    }
    CHECK(b2Length(b2Body_GetLinearVelocity(ball)) < 0.005f);
    CHECK(b2Distance(b2Body_GetPosition(ball), physics::BORDER_CENTER) < contact_distance);

    b2DestroyWorld(world);
    CHECK(!b2World_IsValid(world));
    return EXIT_SUCCESS;
}
