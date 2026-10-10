#pragma once

#include <chrono>
#include <cstdint>

#include <driver/gpio.h>
#include <driver/i2c_master.h>

namespace drivers::i2c
{
inline constexpr i2c_port_num_t PORT = I2C_NUM_0;
inline constexpr gpio_num_t SCL = GPIO_NUM_18;
inline constexpr gpio_num_t SDA = GPIO_NUM_8;
inline constexpr std::uint32_t FREQUENCY_HZ = 400'000;
inline constexpr std::chrono::milliseconds TIMEOUT{20};
} // namespace drivers::i2c
