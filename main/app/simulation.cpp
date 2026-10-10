#include "simulation.h"

#include <cmath>

#include <esp_check.h>
#include <esp_log.h>

#if CONFIG_QUALIA_MODE_TRACE
#include <array>
#include <cinttypes>
#include <cstdio>

#include <esp_app_trace.h>
#include <esp_timer.h>
#endif

#include "display_helpers.h"
#include "lcd_config.h"
#include "tilt.h"

namespace
{
constexpr const char* TAG = "simulation";
} // namespace

namespace app
{
#if CONFIG_QUALIA_MODE_TRACE
void Simulation::trace_step(std::uint64_t start, std::uint64_t duration)
{
    if (!esp_apptrace_host_is_connected())
    {
        trace_error_reported_ = false;
        return;
    }
    std::array<char, 96> record{};
    const int length = std::snprintf(record.data(), record.size(),
                                     "[%" PRIu64 "] - update_simulation: delta=%" PRIu64 "\n", start, duration);
    if (length <= 0 || static_cast<std::size_t>(length) >= record.size())
    {
        ESP_LOGE(TAG, "Failed to format simulation trace record");
        return;
    }
    const esp_err_t err = esp_apptrace_write(record.data(), length, 0);
    if (err != ESP_OK && !trace_error_reported_)
    {
        ESP_LOGW("update_simulation", "Trace write failed: %s; further failures are suppressed until reconnect",
                 esp_err_to_name(err));
        trace_error_reported_ = true;
    }
}
#endif

Simulation::~Simulation()
{
    destroy();
}

esp_err_t Simulation::create_scene()
{
    border_ = physics::create_border(world_);
    ESP_RETURN_ON_FALSE(b2Body_IsValid(border_), ESP_ERR_NO_MEM, TAG, "Failed to create border body");

    const int inner_radius = static_cast<int>(std::lround(physics::BORDER_RADIUS * drivers::lcd::WIDTH));
    const auto center = display::world_to_panel(physics::BORDER_CENTER.x, physics::BORDER_CENTER.y, drivers::lcd::WIDTH,
                                                drivers::lcd::HEIGHT);
    ESP_RETURN_ON_ERROR(border_view_.create(center, inner_radius), TAG, "Failed to create border view");
    return ball_.create(world_, {0.3f, 0.8f}, on_bounce_ != nullptr);
}

esp_err_t Simulation::init(BounceCallback on_bounce)
{
    ESP_RETURN_ON_FALSE(!is_initialized(), ESP_ERR_INVALID_STATE, TAG, "Simulation is already initialized");
    world_ = physics::create_world();
    ESP_RETURN_ON_FALSE(b2World_IsValid(world_), ESP_ERR_NO_MEM, TAG, "Failed to create physics world");
    on_bounce_ = on_bounce;
    const esp_err_t err = create_scene();
    if (err != ESP_OK)
    {
        destroy();
    }
    return err;
}

bool Simulation::is_initialized() const
{
    return b2World_IsValid(world_);
}

esp_err_t Simulation::step(std::chrono::microseconds now, std::optional<imu::Vec3> up)
{
    ESP_RETURN_ON_FALSE(is_initialized(), ESP_ERR_INVALID_STATE, TAG, "Simulation is not initialized");
    ESP_RETURN_ON_FALSE(now.count() >= 0 && (!last_update_ || now >= *last_update_), ESP_ERR_INVALID_ARG, TAG,
                        "Simulation time must be monotonic");
    if (!last_update_)
    {
        last_update_ = now;
        return ESP_OK;
    }
    const auto elapsed = now - *last_update_;
    const std::uint32_t steps = step_clock_.advance(static_cast<std::uint64_t>(elapsed.count()));
    last_update_ = now;
    if (steps > 0 && up)
    {
        b2World_SetGravity(world_, tilt::gravity_from_accel(*up, tilt::BOARD_AXIS_MAP, physics::GRAVITY_MAGNITUDE));
    }
    for (std::uint32_t i = 0; i < steps; ++i)
    {
#if CONFIG_QUALIA_MODE_TRACE
        const std::uint64_t step_start = esp_timer_get_time();
#endif
        physics::step(world_);
        if (on_bounce_ != nullptr)
        {
            const auto bounce = physics::strongest_bounce(b2World_GetContactEvents(world_), ball_.body(), border_);
            if (bounce)
            {
                on_bounce_(*bounce);
            }
        }
#if CONFIG_QUALIA_MODE_TRACE
        trace_step(step_start, esp_timer_get_time() - step_start);
#endif
    }
    return steps > 0 ? ball_.draw() : ESP_OK;
}

void Simulation::destroy()
{
    ball_.destroy();
    border_view_.destroy();
    if (b2World_IsValid(world_))
    {
        b2DestroyWorld(world_);
    }
    world_ = b2_nullWorldId;
    border_ = b2_nullBodyId;
    step_clock_ = physics::FixedStepClock{physics::STEPS_PER_SECOND, MAX_STEPS_PER_UPDATE};
    last_update_.reset();
    on_bounce_ = nullptr;
#if CONFIG_QUALIA_MODE_TRACE
    trace_error_reported_ = false;
#endif
}
} // namespace app
