#include "ball.h"

#include <cassert>
#include <cmath>

#include <box2d/box2d.h>
#include <esp_check.h>
#include <esp_err.h>

#include "display_helpers.h"
#include "lcd_controller.h"
#include "physics_world.h"

#define LOG_TAG "ball"

esp_err_t ball_create(ball_t* ball, float x, float y, b2WorldId world, bool report_hits)
{
    ESP_RETURN_ON_FALSE(ball != NULL, ESP_ERR_INVALID_ARG, LOG_TAG, "Ball is NULL");

    // Draw the circle at the physics size.
    const int diameter = static_cast<int>(std::lround(2.0f * physics::BALL_RADIUS * LCD_H_RES));
    ESP_RETURN_ON_ERROR(ball->view.create(diameter), LOG_TAG, "Failed to create ball view");

    const b2BodyId body = physics::create_ball(world, {x, y}, report_hits);
    if (!b2Body_IsValid(body))
    {
        ball->view.destroy();
        ESP_LOGE(LOG_TAG, "Failed to create ball body");
        return ESP_ERR_NO_MEM;
    }

    ball->body = body;
    ball_draw(ball);

    return ESP_OK;
}

void ball_draw(ball_t* ball)
{
    assert(ball != NULL);

    const b2Vec2 position = b2Body_GetPosition(ball->body);
    ball->view.move_to(display::world_to_panel(position.x, position.y, LCD_H_RES, LCD_V_RES));
}