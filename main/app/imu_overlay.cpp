#include "imu_overlay.h"

#include <cstdint>

#include <esp_timer.h>

#include "gui/imu_readout.h"

namespace
{
constexpr std::uint64_t REFRESH_PERIOD_US = 250'000;

gui::ImuReadout g_readout;
std::uint64_t g_last_refresh_us = 0;
const app::BoardImu* g_imu = nullptr;
} // namespace

esp_err_t imu_overlay_create(const app::BoardImu& imu)
{
    g_imu = &imu;
    return g_readout.create();
}

void imu_overlay_update()
{
    const std::uint64_t now = esp_timer_get_time();
    if (g_imu == nullptr || now - g_last_refresh_us < REFRESH_PERIOD_US)
    {
        return;
    }
    g_last_refresh_us = now;

    const auto state = g_imu->latest();
    if (!state)
    {
        return;
    }
    g_readout.show(state->up, state->motion.gyro, state->magnetic.value_or(imu::Vec3{}));
}
