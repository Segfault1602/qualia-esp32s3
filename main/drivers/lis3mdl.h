#pragma once

#include <expected>

#include <esp_err.h>

#include "sensor_types.h"

namespace drivers
{
/**
 * @brief The board's LIS3MDL magnetometer on the shared I2C bus (STEMMA QT).
 */
class Lis3mdl
{
  public:
    Lis3mdl() = delete;

    /**
     * @brief Requires I2cBus.
     */
    static esp_err_t init();
    static bool is_initialized();

    /**
     * @brief Latest magnetic field in microtesla.
     */
    static std::expected<imu::Vec3, esp_err_t> read();
};
} // namespace drivers
