#pragma once

#include <cstdint>
#include <expected>

#include "register_bus.h"
#include "sensor_types.h"

namespace imu
{
/**
 * @brief LSM6DSOX accelerometer and gyroscope over any RegisterBus, wrapping ST's lsm6dsox-pid driver.
 *
 * drivers::Lsm6dsox owns the board's instance.
 */
class Lsm6dsoxDevice
{
  public:
    static constexpr std::uint16_t DEFAULT_ADDRESS = 0x6A;
    static constexpr std::uint8_t DEVICE_ID = 0x6C;

    explicit Lsm6dsoxDevice(RegisterBus& bus);

    /**
     * @brief Reset the device, then run the accelerometer (+/-2 g) and gyroscope (+/-500 dps) at 104 Hz.
     */
    std::expected<void, Error> init();

    std::expected<MotionSample, Error> read();

  private:
    RegisterBus& bus_;
};
} // namespace imu
