#pragma once

#include <cstdint>
#include <expected>

#include "register_bus.h"
#include "sensor_types.h"

namespace imu
{
/**
 * @brief LIS3MDL magnetometer over any RegisterBus, wrapping ST's lis3mdl-pid driver.
 *
 * drivers::Lis3mdl owns the board's instance.
 */
class Lis3mdlDevice
{
  public:
    static constexpr std::uint16_t DEFAULT_ADDRESS = 0x1C;
    static constexpr std::uint8_t DEVICE_ID = 0x3D;

    explicit Lis3mdlDevice(RegisterBus& bus);

    /**
     * @brief Reset the device, then measure continuously at 80 Hz in ultra-high-performance mode (+/-4 gauss).
     */
    std::expected<void, Error> init();

    /**
     * @brief Latest magnetic field in microtesla.
     */
    std::expected<Vec3, Error> read();

  private:
    RegisterBus& bus_;
};
} // namespace imu
