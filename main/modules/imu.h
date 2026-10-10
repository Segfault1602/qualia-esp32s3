#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

#include <esp_err.h>
#include <esp_log.h>

#include "gravity_estimator.h"
#include "latest_value.h"
#include "modules/sensor_concepts.h"
#include "sensor_types.h"

namespace modules
{
/**
 * @brief Newest IMU readings, in the sensor frame.
 */
struct ImuState
{
    imu::Vec3 up; // m/s^2, at-rest accelerometer reading (+1 g up), fused with the gyroscope
    imu::MotionSample motion;
    std::optional<imu::Vec3> magnetic; // uT, absent when the magnetometer is unavailable
};

/**
 * @brief Owns a Motion and a Magnetic sensor. Each step samples Motion and fuses the gyroscope into the "up"
 * estimate; every tenth step also samples Magnetic when it is available.
 */
template <AccelGyroSensor Motion, MagnetometerSensor Magnetic>
class Imu
{
  public:
    /**
     * @brief Intended step period.
     */
    static constexpr std::chrono::milliseconds PERIOD{10};

    Imu() = default;
    Imu(const Imu&) = delete;
    Imu& operator=(const Imu&) = delete;

    /**
     * @brief Fails when Motion is unavailable; a missing Magnetic only disables the magnetic readings.
     */
    esp_err_t init()
    {
        const esp_err_t motion = Motion::init();
        if (motion != ESP_OK)
        {
            return motion;
        }
        const esp_err_t magnetic = Magnetic::init();
        if (magnetic != ESP_OK)
        {
            ESP_LOGW(TAG, "Magnetometer disabled (%s)", esp_err_to_name(magnetic));
        }
        initialized_ = true;
        return ESP_OK;
    }

    bool is_initialized() const
    {
        return initialized_;
    }

    void step(std::chrono::microseconds now)
    {
        const std::uint32_t count = step_count_++;

        const auto motion = Motion::read();
        if (!motion)
        {
            if (!read_error_reported_)
            {
                ESP_LOGW(TAG, "Motion read failed: %s; further failures are suppressed until it recovers",
                         esp_err_to_name(motion.error()));
                read_error_reported_ = true;
            }
            return;
        }
        read_error_reported_ = false;

        const float dt_s = last_sample_ ? std::chrono::duration<float>(now - *last_sample_).count() : 0.0f;
        last_sample_ = now;
        state_.motion = *motion;
        state_.up = gravity_estimator_.update(motion->accel, motion->gyro, dt_s);

        if (Magnetic::is_initialized() && count % MAGNETIC_DIVIDER == 0)
        {
            const auto field = Magnetic::read();
            if (field)
            {
                state_.magnetic = *field;
            }
        }
        latest_.publish(state_);

#if CONFIG_QUALIA_IMU_DEBUG
        if (count % DEBUG_LOG_DIVIDER == 0)
        {
            const imu::Vec3 magnetic = state_.magnetic.value_or(imu::Vec3{});
            ESP_LOGI(TAG, "accel %6.2f %6.2f %6.2f | gyro %6.3f %6.3f %6.3f | mag %6.1f %6.1f %6.1f", state_.up.x,
                     state_.up.y, state_.up.z, state_.motion.gyro.x, state_.motion.gyro.y, state_.motion.gyro.z,
                     magnetic.x, magnetic.y, magnetic.z);
        }
#endif
    }

    /**
     * @brief Newest readings, or nullopt until the first successful step. Safe from any task.
     */
    std::optional<ImuState> latest() const
    {
        return latest_.load();
    }

  private:
    static constexpr const char* TAG = "imu";
    static constexpr std::uint32_t MAGNETIC_DIVIDER = 10;
    static constexpr std::uint32_t DEBUG_LOG_DIVIDER = 50;
    static constexpr float GRAVITY_CORRECTION_TIME_CONSTANT_S = 0.5f;

    LatestValue<ImuState> latest_;
    filter::GravityEstimator gravity_estimator_{GRAVITY_CORRECTION_TIME_CONSTANT_S};
    ImuState state_;
    std::optional<std::chrono::microseconds> last_sample_;
    std::uint32_t step_count_ = 0;
    bool initialized_ = false;
    bool read_error_reported_ = false;
};
} // namespace modules
