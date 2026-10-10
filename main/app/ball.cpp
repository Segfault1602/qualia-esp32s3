#include "ball.h"

#include <cmath>

#include <box2d/box2d.h>
#include <esp_check.h>
#include <esp_err.h>

#include "display_helpers.h"
#include "lcd_config.h"
#include "physics_world.h"

namespace
{
constexpr const char* TAG = "ball";
} // namespace

namespace app
{
Ball::~Ball()
{
    destroy();
}

esp_err_t Ball::create(b2WorldId world, b2Vec2 position, bool report_hits)
{
    ESP_RETURN_ON_FALSE(!b2Body_IsValid(body_), ESP_ERR_INVALID_STATE, TAG, "Ball already created");
    ESP_RETURN_ON_FALSE(b2World_IsValid(world), ESP_ERR_INVALID_ARG, TAG, "Invalid ball world");
    ESP_RETURN_ON_FALSE(std::isfinite(position.x) && std::isfinite(position.y), ESP_ERR_INVALID_ARG, TAG,
                        "Invalid ball position");
    const int diameter = static_cast<int>(std::lround(2.0f * physics::BALL_RADIUS * drivers::lcd::WIDTH));
    ESP_RETURN_ON_ERROR(view_.create(diameter), TAG, "Failed to create ball view");

    body_ = physics::create_ball(world, position, report_hits);
    if (!b2Body_IsValid(body_))
    {
        destroy();
        ESP_LOGE(TAG, "Failed to create ball body");
        return ESP_ERR_NO_MEM;
    }
    const esp_err_t err = draw();
    if (err != ESP_OK)
    {
        destroy();
    }
    return err;
}

esp_err_t Ball::draw()
{
    ESP_RETURN_ON_FALSE(b2Body_IsValid(body_), ESP_ERR_INVALID_STATE, TAG, "Ball is not created");
    const b2Vec2 position = b2Body_GetPosition(body_);
    return view_.move_to(display::world_to_panel(position.x, position.y, drivers::lcd::WIDTH, drivers::lcd::HEIGHT));
}

void Ball::destroy()
{
    view_.destroy();
    if (b2Body_IsValid(body_))
    {
        b2DestroyBody(body_);
    }
    body_ = b2_nullBodyId;
}
} // namespace app