#include "ball.h"

#include <box2d/box2d.h>
#include <esp_check.h>
#include <esp_err.h>
#include <lvgl.h>

#define LOG_TAG "ball"

#define DRAW_BUF_WIDTH 50
#define DRAW_BUF_HEIGHT 50

namespace
{
// Top left corner is (0, 0) in LVGL, but (0, 1) in Box2D
// Bottom right corner is (480, 0) in LVGL, but (1, 0) in Box2D
constexpr uint32_t box2lvgl_x(float x)
{
    return x * 480;
}

constexpr uint32_t box2lvgl_y(float y)
{
    return 480 - y * 480;
}
} // namespace

esp_err_t ball_create(ball_t* ball, float x, float y, b2World& world)
{
    ESP_RETURN_ON_FALSE(ball != NULL, ESP_ERR_INVALID_ARG, LOG_TAG, "Ball is NULL");

    ball->x = box2lvgl_x(x);
    ball->y = box2lvgl_y(y);

    lv_draw_buf_t* draw_buf = lv_draw_buf_create(DRAW_BUF_WIDTH, DRAW_BUF_HEIGHT, LV_COLOR_FORMAT_ARGB8888, 0);
    lv_obj_t* canvas = lv_canvas_create(lv_screen_active());
    lv_canvas_set_draw_buf(canvas, draw_buf);
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_TRANSP);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    // dsc.bg_grad.dir = LV_GRAD_DIR_HOR;
    dsc.bg_opa = LV_OPA_COVER;
    // dsc.bg_grad.stops[0].color = lv_palette_main(LV_PALETTE_GREEN);
    // dsc.bg_grad.stops[0].opa = LV_OPA_100;
    // dsc.bg_grad.stops[1].color = lv_palette_darken(LV_PALETTE_GREEN, 3);
    // dsc.bg_grad.stops[1].opa = LV_OPA_90;
    dsc.bg_color = lv_palette_main(LV_PALETTE_GREEN);
    dsc.border_width = 5;
    dsc.border_opa = LV_OPA_90;
    dsc.border_color = lv_palette_darken(LV_PALETTE_GREEN, 3);
    dsc.radius = LV_RADIUS_CIRCLE;

    lv_area_t coords = {5, 5, DRAW_BUF_WIDTH - 5, DRAW_BUF_HEIGHT - 5};

    lv_draw_rect(&layer, &dsc, &coords);

    lv_canvas_finish_layer(canvas, &layer);

    lv_obj_set_pos(canvas, x, y);

    ball->obj = canvas;

    b2BodyDef body_def;
    body_def.type = b2_dynamicBody;
    body_def.position.Set(x, y);
    b2Body* body = world.CreateBody(&body_def);

    b2CircleShape shape;
    shape.m_radius = 25.f / 480.f;
    b2FixtureDef fixture_def;
    fixture_def.shape = &shape;
    fixture_def.density = 1.0f;
    fixture_def.friction = 0.3f;
    fixture_def.restitution = 0.5f;

    body->CreateFixture(&fixture_def);

    ball->body = body;

    return ESP_OK;
}

void ball_destroy(ball_t* ball)
{
    assert(ball != NULL);

    lv_draw_buf_t* draw_buf = lv_canvas_get_draw_buf(ball->obj);
    lv_draw_buf_destroy(draw_buf);

    lv_obj_del(ball->obj);
}

void ball_draw(ball_t* ball)
{
    assert(ball != NULL);

    b2Vec2 position = ball->body->GetPosition();
    ball->x = box2lvgl_x(position.x);
    ball->y = box2lvgl_x(position.y);

    uint32_t x_offset = DRAW_BUF_WIDTH / 2 + 2;
    uint32_t y_offset = DRAW_BUF_HEIGHT / 2 + 2;

    lv_obj_set_pos(ball->obj, ball->x - x_offset, ball->y - y_offset);
    // lv_obj_invalidate(ball->obj);
}