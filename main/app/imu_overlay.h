#pragma once

#include <chrono>
#include <optional>

#include <esp_err.h>

#include "gui/imu_readout.h"
#include "modules/imu.h"

namespace app
{
/**
 * @brief Owns an IMU readout, refreshed at 4 Hz from caller-provided snapshots.
 *
 * All operations belong to the UI task. Destroy while the active screen is valid.
 */
class ImuOverlay
{
  public:
    ImuOverlay() = default;
    ImuOverlay(const ImuOverlay&) = delete;
    ImuOverlay& operator=(const ImuOverlay&) = delete;

    esp_err_t init();
    bool is_initialized() const;
    esp_err_t step(std::chrono::microseconds now, const std::optional<modules::ImuState>& snapshot);
    void destroy();

  private:
    static constexpr std::chrono::milliseconds REFRESH_PERIOD{250};
    gui::ImuReadout readout_;
    std::chrono::microseconds last_refresh_{};
    bool initialized_ = false;
};
} // namespace app
