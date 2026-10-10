#pragma once

#include <concepts>
#include <expected>

#include <esp_err.h>

#include "sensor_types.h"

namespace modules
{
/**
 * @brief A static-only accelerometer/gyroscope driver.
 */
template <typename T>
concept AccelGyroSensor = requires {
    { T::init() } -> std::same_as<esp_err_t>;
    { T::is_initialized() } -> std::same_as<bool>;
    { T::read() } -> std::same_as<std::expected<imu::MotionSample, esp_err_t>>;
};

/**
 * @brief A static-only magnetometer driver; read() returns the field in microtesla.
 */
template <typename T>
concept MagnetometerSensor = requires {
    { T::init() } -> std::same_as<esp_err_t>;
    { T::is_initialized() } -> std::same_as<bool>;
    { T::read() } -> std::same_as<std::expected<imu::Vec3, esp_err_t>>;
};
} // namespace modules
