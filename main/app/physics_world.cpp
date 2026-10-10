#include "physics_world.h"

#include <array>
#include <cmath>

namespace physics
{
namespace
{
constexpr int BORDER_SEGMENTS = 64;
constexpr float BORDER_FRICTION = 0.6f;
constexpr float BALL_DENSITY = 1.0f;
constexpr float BALL_FRICTION = 0.6f;
constexpr float BALL_RESTITUTION = 0.95f;
// Box2D's default only bounces impacts above 1 unit/s, a screen width per second here, which flattens most bounces.
constexpr float RESTITUTION_THRESHOLD = 0.2f;
constexpr float BALL_ROLLING_RESISTANCE = 0.02f;
// Stands in for friction against the screen surface, which the 2D world does not model; without it the
// ball coasts forever when the screen lies flat.
constexpr float BALL_LINEAR_DAMPING = 0.3f;
}

b2WorldId create_world()
{
    b2WorldDef world_def = b2DefaultWorldDef();
    world_def.gravity = DEFAULT_GRAVITY;
    world_def.restitutionThreshold = RESTITUTION_THRESHOLD;
    return b2CreateWorld(&world_def);
}

b2BodyId create_border(b2WorldId world)
{
    const b2BodyDef body_def = b2DefaultBodyDef();
    const b2BodyId border = b2CreateBody(world, &body_def);
    if (!b2Body_IsValid(border))
    {
        return b2_nullBodyId;
    }

    // Chain segments collide on their right side; clockwise points face the center.
    std::array<b2Vec2, BORDER_SEGMENTS> points{};
    for (int i = 0; i < BORDER_SEGMENTS; ++i)
    {
        const float angle = -2.0f * B2_PI * static_cast<float>(i) / BORDER_SEGMENTS;
        points[i] = {BORDER_CENTER.x + BORDER_RADIUS * std::cos(angle),
                     BORDER_CENTER.y + BORDER_RADIUS * std::sin(angle)};
    }
    b2SurfaceMaterial material = b2DefaultSurfaceMaterial();
    material.friction = BORDER_FRICTION;
    b2ChainDef chain_def = b2DefaultChainDef();
    chain_def.points = points.data();
    chain_def.count = BORDER_SEGMENTS;
    chain_def.materials = &material;
    chain_def.materialCount = 1;
    chain_def.isLoop = true;
    const b2ChainId chain = b2CreateChain(border, &chain_def);
    if (!b2Chain_IsValid(chain))
    {
        b2DestroyBody(border);
        return b2_nullBodyId;
    }
    return border;
}

b2BodyId create_ball(b2WorldId world, b2Vec2 position)
{
    b2BodyDef body_def = b2DefaultBodyDef();
    body_def.type = b2_dynamicBody;
    body_def.position = position;
    body_def.linearDamping = BALL_LINEAR_DAMPING;
    // Changing gravity does not wake a sleeping body, so the ball stays awake to follow the tilt.
    body_def.enableSleep = false;
    const b2BodyId ball = b2CreateBody(world, &body_def);
    if (!b2Body_IsValid(ball))
    {
        return b2_nullBodyId;
    }

    b2ShapeDef shape_def = b2DefaultShapeDef();
    shape_def.density = BALL_DENSITY;
    shape_def.material.friction = BALL_FRICTION;
    shape_def.material.restitution = BALL_RESTITUTION;
    shape_def.material.rollingResistance = BALL_ROLLING_RESISTANCE;
    const b2Circle circle{{0.0f, 0.0f}, BALL_RADIUS};
    const b2ShapeId shape = b2CreateCircleShape(ball, &shape_def, &circle);
    if (!b2Shape_IsValid(shape))
    {
        b2DestroyBody(ball);
        return b2_nullBodyId;
    }
    return ball;
}

void step(b2WorldId world)
{
    b2World_Step(world, 1.0f / STEPS_PER_SECOND, SUB_STEPS);
}
}
