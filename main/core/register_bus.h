#pragma once

#include <cstdint>
#include <span>

namespace imu
{
/**
 * @brief Register-addressed transport for one sensor.
 *
 * Both calls return 0 on success, matching the ST register driver convention.
 */
class RegisterBus
{
  public:
    virtual ~RegisterBus() = default;
    virtual std::int32_t read(std::uint8_t reg, std::span<std::uint8_t> data) = 0;
    virtual std::int32_t write(std::uint8_t reg, std::span<const std::uint8_t> data) = 0;
};
} // namespace imu
