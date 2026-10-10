#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

#include <esp_err.h>
#include <sdkconfig.h>

#include "ball.h"
#include "fixed_step.h"
#include "gui/border_view.h"
#include "physics_world.h"
#include "sensor_types.h"

namespace app
{
using BounceCallback = void (*)(const physics::Bounce&);

/**
 * @brief Owns the physics world and scene views. All operations belong to the UI task.
 *
 * The first step anchors the clock; later steps use monotonic time since boot.
 * A missing up vector keeps the current gravity, initially fixed downward.
 * Destroy before LVGL or its active screen is destroyed.
 */
class Simulation
{
  public:
    Simulation() = default;
    ~Simulation();
    Simulation(const Simulation&) = delete;
    Simulation& operator=(const Simulation&) = delete;

    esp_err_t init(BounceCallback on_bounce = nullptr);
    bool is_initialized() const;
    esp_err_t step(std::chrono::microseconds now, std::optional<imu::Vec3> up = std::nullopt);
    void destroy();

  private:
    esp_err_t create_scene();
#if CONFIG_QUALIA_MODE_TRACE
    void trace_step(std::uint64_t start, std::uint64_t duration);
    bool trace_error_reported_ = false;
#endif

    static constexpr std::uint32_t MAX_STEPS_PER_UPDATE = 4;
    b2WorldId world_ = b2_nullWorldId;
    b2BodyId border_ = b2_nullBodyId;
    gui::BorderView border_view_;
    Ball ball_;
    physics::FixedStepClock step_clock_{physics::STEPS_PER_SECOND, MAX_STEPS_PER_UPDATE};
    std::optional<std::chrono::microseconds> last_update_;
    BounceCallback on_bounce_ = nullptr;
};
} // namespace app