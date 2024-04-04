#include "simulation.h"

#include <box2d/box2d.h>
#include <esp_app_trace.h>
#include <esp_app_trace_util.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <lvgl.h>

#include "ball.h"

namespace
{

b2Vec2 g_gravity(0.0f, -10.0f);
b2World g_world(g_gravity);

ball_t g_ball;

uint64_t g_last_update = 0;
constexpr uint32_t g_target_fps = 60;
uint64_t g_update_freq = 1000000 / g_target_fps;

constexpr uint32_t box2lvgl_x(float x)
{
    return x * 480;
}

constexpr uint32_t box2lvgl_y(float y)
{
    return y * 480;
}

} // namespace

void create_ground()
{
    constexpr uint8_t POINT_COUNT = 2;
    b2Vec2 points[POINT_COUNT] = {
        {0, 0.1},
        {1, 0.1},
    };

    b2BodyDef body_def;
    b2Body* ground = g_world.CreateBody(&body_def);

    b2EdgeShape edge_shape;
    edge_shape.SetTwoSided(points[0], points[1]);

    ground->CreateFixture(&edge_shape, 0.0f);

    static lv_point_precise_t line_points[POINT_COUNT];
    for (uint8_t i = 0; i < POINT_COUNT; i++)
    {
        line_points[i].x = box2lvgl_x(points[i].x);
        line_points[i].y = box2lvgl_y(points[i].y);
    }
    static lv_style_t style_line;
    lv_style_init(&style_line);
    lv_style_set_line_width(&style_line, 2);
    lv_style_set_line_color(&style_line, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_line_rounded(&style_line, true);

    lv_obj_t* line = lv_line_create(lv_screen_active());
    lv_line_set_points(line, line_points, 2);
    lv_obj_add_style(line, &style_line, 0);
}

void create_ball()
{
    ball_create(&g_ball, 0.5, 0.9, g_world);

} // namespace

void init_simulation()
{
    create_ground();
    create_ball();
}

void update_simulation()
{
    uint64_t now = esp_timer_get_time();
    if (now - g_last_update > g_update_freq)
    {
        g_world.Step(1.0f / g_target_fps, 4, 4);
        uint64_t end = esp_timer_get_time();
        uint64_t delta = end - now;
        g_last_update = now;

        draw_simulation();

        char ptr[64];
        sprintf(ptr, "[%llu] - update_simulation: delta=%llu\n", now, delta);
        esp_err_t err = esp_apptrace_write(ESP_APPTRACE_DEST_TRAX, ptr, strlen(ptr), 100);
        if (err != ESP_OK)
        {
            ESP_LOGE("update_simulation", "esp_apptrace_write failed: %s", esp_err_to_name(err));
        }
    }
}

void draw_simulation()
{
    ball_draw(&g_ball);
}
