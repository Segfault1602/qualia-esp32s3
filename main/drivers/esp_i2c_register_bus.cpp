#include "esp_i2c_register_bus.h"

#include <array>

#include <esp_check.h>

#include "i2c_config.h"

namespace
{
constexpr const char* TAG = "i2c_register_bus";
} // namespace

EspI2cRegisterBus::~EspI2cRegisterBus()
{
    if (device_ != nullptr)
    {
        ESP_ERROR_CHECK(i2c_master_bus_rm_device(device_));
    }
}

esp_err_t EspI2cRegisterBus::open(i2c_master_bus_handle_t bus, std::uint16_t address)
{
    ESP_RETURN_ON_FALSE(device_ == nullptr, ESP_ERR_INVALID_STATE, TAG, "Device 0x%02x is already open", address);
    ESP_RETURN_ON_FALSE(bus != nullptr, ESP_ERR_INVALID_ARG, TAG, "I2C bus is NULL");

    i2c_device_config_t config{};
    config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    config.device_address = address;
    config.scl_speed_hz = drivers::i2c::FREQUENCY_HZ;
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &config, &device_), TAG, "Failed to add device 0x%02x", address);
    return ESP_OK;
}

esp_err_t EspI2cRegisterBus::to_esp_err(imu::Error error) const
{
    switch (error)
    {
    case imu::Error::Bus:
        return last_error_ != ESP_OK ? last_error_ : ESP_FAIL;
    case imu::Error::WrongDevice:
        return ESP_ERR_NOT_SUPPORTED;
    case imu::Error::Timeout:
        return ESP_ERR_TIMEOUT;
    }
    return ESP_FAIL;
}

std::int32_t EspI2cRegisterBus::read(std::uint8_t reg, std::span<std::uint8_t> data)
{
    last_error_ =
        i2c_master_transmit_receive(device_, &reg, 1, data.data(), data.size(), drivers::i2c::TIMEOUT.count());
    return last_error_;
}

std::int32_t EspI2cRegisterBus::write(std::uint8_t reg, std::span<const std::uint8_t> data)
{
    std::array buffers{
        i2c_master_transmit_multi_buffer_info_t{&reg, 1},
        i2c_master_transmit_multi_buffer_info_t{data.data(), data.size()},
    };
    last_error_ =
        i2c_master_multi_buffer_transmit(device_, buffers.data(), buffers.size(), drivers::i2c::TIMEOUT.count());
    return last_error_;
}
