#pragma once

#include <cstdint>

#include <driver/i2c_master.h>
#include <esp_err.h>

#include "register_bus.h"
#include "sensor_types.h"

namespace drivers
{
class I2cBus;
} // namespace drivers

/**
 * @brief RegisterBus for one device on an ESP-IDF I2C master bus.
 *
 * Attach it with drivers::I2cBus::open(); the device is removed from the bus on destruction.
 */
class EspI2cRegisterBus final : public imu::RegisterBus
{
  public:
    EspI2cRegisterBus() = default;
    ~EspI2cRegisterBus() override;
    EspI2cRegisterBus(const EspI2cRegisterBus&) = delete;
    EspI2cRegisterBus& operator=(const EspI2cRegisterBus&) = delete;

    std::int32_t read(std::uint8_t reg, std::span<std::uint8_t> data) override;
    std::int32_t write(std::uint8_t reg, std::span<const std::uint8_t> data) override;

    /**
     * @brief Result of the most recent transfer.
     */
    esp_err_t last_error() const
    {
        return last_error_;
    }

    /**
     * @brief ESP-IDF error for a failure reported by a sensor driver on this bus.
     */
    esp_err_t to_esp_err(imu::Error error) const;

  private:
    friend class drivers::I2cBus;

    /**
     * @param address 7-bit device address
     */
    esp_err_t open(i2c_master_bus_handle_t bus, std::uint16_t address);

    i2c_master_dev_handle_t device_ = nullptr;
    esp_err_t last_error_ = ESP_OK;
};
