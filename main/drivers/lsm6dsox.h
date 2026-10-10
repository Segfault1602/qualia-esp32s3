#pragma once

#include <expected>

#include <esp_err.h>

#include "sensor_types.h"

namespace drivers
{
/**
 * @brief The board's LSM6DSOX accelerometer/gyroscope on the shared I2C bus (STEMMA QT).
 */
class Lsm6dsox
{
  public:
    Lsm6dsox() = delete;

    /**
     * @brief Requires I2cBus.
     */
    static esp_err_t init();
    static bool is_initialized();

    static std::expected<imu::MotionSample, esp_err_t> read();
};
} // namespace drivers
