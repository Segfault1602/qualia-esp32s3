#include "imu_overlay.h"

#include <esp_check.h>

namespace app
{
esp_err_t ImuOverlay::init()
{
    ESP_RETURN_ON_FALSE(!initialized_, ESP_ERR_INVALID_STATE, "imu_overlay", "IMU overlay is already initialized");
    ESP_RETURN_ON_ERROR(readout_.create(), "imu_overlay", "Failed to create IMU readout");
    initialized_ = true;
    return ESP_OK;
}

bool ImuOverlay::is_initialized() const
{
    return initialized_;
}

esp_err_t ImuOverlay::step(std::chrono::microseconds now, const std::optional<modules::ImuState>& snapshot)
{
    ESP_RETURN_ON_FALSE(initialized_, ESP_ERR_INVALID_STATE, "imu_overlay", "IMU overlay is not initialized");
    ESP_RETURN_ON_FALSE(now >= last_refresh_, ESP_ERR_INVALID_ARG, "imu_overlay", "IMU time must be monotonic");
    if (now - last_refresh_ < REFRESH_PERIOD)
    {
        return ESP_OK;
    }
    last_refresh_ = now;
    if (!snapshot)
    {
        return ESP_OK;
    }
    return readout_.show(snapshot->up, snapshot->motion.gyro, snapshot->magnetic.value_or(imu::Vec3{}));
}

void ImuOverlay::destroy()
{
    readout_.destroy();
    last_refresh_ = std::chrono::microseconds::zero();
    initialized_ = false;
}
} // namespace app
