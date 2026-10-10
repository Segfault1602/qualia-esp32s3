#include "simulation.h"

#include <cmath>

#include <box2d/box2d.h>
#include <esp_check.h>
#include <esp_log.h>
#include <esp_timer.h>

#if CONFIG_QUALIA_MODE_TRACE
#include <esp_app_trace.h>
#include <inttypes.h>
#include <stdio.h>
#endif

#include "ball.h"
#include "display_helpers.h"
#include "fixed_step.h"
#include "gui/border_view.h"
#include "lcd_controller.h"
#include "physics_world.h"
#include "tilt.h"

#if CONFIG_QUALIA_IMU_DEBUG
#include "imu_overlay.h"
#endif

namespace
{

b2WorldId g_world = b2_nullWorldId;

ball_t g_ball;

constexpr uint32_t MAX_STEPS_PER_UPDATE = 4;
physics::FixedStepClock g_step_clock(physics::STEPS_PER_SECOND, MAX_STEPS_PER_UPDATE);
uint64_t g_last_update = 0;
const app::BoardImu* g_imu = nullptr;

#if CONFIG_QUALIA_MODE_TRACE
void trace_step(uint64_t start, uint64_t duration)
{
    static bool trace_error_reported = false;
    if (!esp_apptrace_host_is_connected())
    {
        trace_error_reported = false;
        return;
    }
    char record[96];
    const int length = snprintf(record, sizeof(record), "[%" PRIu64 "] - update_simulation: delta=%" PRIu64 "\n",
                                start, duration);
    assert(length > 0 && static_cast<size_t>(length) < sizeof(record));
    const esp_err_t err = esp_apptrace_write(record, length, 0);
    if (err != ESP_OK && !trace_error_reported)
    {
        ESP_LOGW("update_simulation", "Trace write failed: %s; further failures are suppressed until reconnect",
                 esp_err_to_name(err));
        trace_error_reported = true;
    }
}
#endif

} // namespace

esp_err_t create_border()
{
    const b2BodyId border = physics::create_border(g_world);
    ESP_RETURN_ON_FALSE(b2Body_IsValid(border), ESP_ERR_NO_MEM, "simulation", "Failed to create border body");

    const int inner_radius = static_cast<int>(std::lround(physics::BORDER_RADIUS * LCD_H_RES));
    const auto center = display::world_to_panel(physics::BORDER_CENTER.x, physics::BORDER_CENTER.y, LCD_H_RES,
                                                LCD_V_RES);
    const esp_err_t err = gui::create_border_ring(center, inner_radius);
    if (err != ESP_OK)
    {
        b2DestroyBody(border);
    }
    return err;
}

esp_err_t create_ball()
{
    return ball_create(&g_ball, 0.3, 0.8, g_world);
}

esp_err_t init_simulation(const app::BoardImu& imu)
{
    g_imu = &imu;
    g_world = physics::create_world();
    ESP_RETURN_ON_FALSE(b2World_IsValid(g_world), ESP_ERR_NO_MEM, "simulation", "Failed to create physics world");
    ESP_RETURN_ON_ERROR(create_border(), "simulation", "Failed to initialize border");
    ESP_RETURN_ON_ERROR(create_ball(), "simulation", "Failed to initialize ball");
#if CONFIG_QUALIA_IMU_DEBUG
    ESP_RETURN_ON_ERROR(imu_overlay_create(imu), "simulation", "Failed to initialize IMU overlay");
#endif
    g_last_update = esp_timer_get_time();
    return ESP_OK;
}

void update_simulation()
{
    const uint64_t now = esp_timer_get_time();
    const uint32_t steps = g_step_clock.advance(now - g_last_update);
    g_last_update = now;
    if (steps > 0)
    {
        const auto imu_state = g_imu->latest();
        if (imu_state)
        {
            b2World_SetGravity(g_world, tilt::gravity_from_accel(imu_state->up, tilt::BOARD_AXIS_MAP,
                                                                 physics::GRAVITY_MAGNITUDE));
        }
    }
    for (uint32_t i = 0; i < steps; ++i)
    {
#if CONFIG_QUALIA_MODE_TRACE
        const uint64_t step_start = esp_timer_get_time();
#endif
        physics::step(g_world);
#if CONFIG_QUALIA_MODE_TRACE
        trace_step(step_start, esp_timer_get_time() - step_start);
#endif
    }
    if (steps > 0)
    {
        ball_draw(&g_ball);
    }
#if CONFIG_QUALIA_IMU_DEBUG
    imu_overlay_update();
#endif
}
