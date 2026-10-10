#pragma once

#include "sensor_types.h"

namespace filter
{
/**
 * @brief Complementary filter that tracks the at-rest accelerometer reading (+1 g pointing up).
 *
 * The gyroscope rotates the estimate as the device turns, so it follows fast rotations without lag. The
 * accelerometer slowly pulls it back, which removes gyroscope drift and filters out hand jitter.
 */
class GravityEstimator
{
  public:
    explicit GravityEstimator(float correction_time_constant_s)
        : correction_time_constant_s_(correction_time_constant_s)
    {
    }

    /**
     * @param accel Specific force in m/s^2, sensor frame
     * @param gyro Angular rate in rad/s, sensor frame
     * @param dt_s Time since the previous sample. The first sample primes the estimate.
     */
    const imu::Vec3& update(const imu::Vec3& accel, const imu::Vec3& gyro, float dt_s)
    {
        if (!primed_)
        {
            estimate_ = accel;
            primed_ = true;
            return estimate_;
        }
        // A world-fixed vector seen from a frame rotating at gyro changes by -gyro x estimate.
        const imu::Vec3 cross{gyro.y * estimate_.z - gyro.z * estimate_.y, gyro.z * estimate_.x - gyro.x * estimate_.z,
                              gyro.x * estimate_.y - gyro.y * estimate_.x};
        estimate_.x -= cross.x * dt_s;
        estimate_.y -= cross.y * dt_s;
        estimate_.z -= cross.z * dt_s;

        const float alpha = dt_s / (correction_time_constant_s_ + dt_s);
        estimate_.x += alpha * (accel.x - estimate_.x);
        estimate_.y += alpha * (accel.y - estimate_.y);
        estimate_.z += alpha * (accel.z - estimate_.z);
        return estimate_;
    }

    const imu::Vec3& value() const
    {
        return estimate_;
    }

  private:
    float correction_time_constant_s_;
    imu::Vec3 estimate_;
    bool primed_ = false;
};
} // namespace filter
