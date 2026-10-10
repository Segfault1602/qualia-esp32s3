#include "i2c_bus.h"

#include <esp_check.h>

#include "defines.h"
#include "esp_i2c_register_bus.h"

namespace
{
constexpr const char* TAG = "i2c_bus";

i2c_master_bus_handle_t g_bus = nullptr;
} // namespace

namespace drivers
{
esp_err_t I2cBus::init()
{
    ESP_RETURN_ON_FALSE(g_bus == nullptr, ESP_ERR_INVALID_STATE, TAG, "I2C bus is already initialized");

    i2c_master_bus_config_t config{};
    config.i2c_port = I2C_MASTER_PORT;
    config.sda_io_num = I2C_MASTER_SDA_IO;
    config.scl_io_num = I2C_MASTER_SCL_IO;
    config.clk_source = I2C_CLK_SRC_DEFAULT;
    config.glitch_ignore_cnt = 7;
    config.flags.enable_internal_pullup = true;
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&config, &g_bus), TAG, "Failed to create I2C bus");
    return ESP_OK;
}

bool I2cBus::is_initialized()
{
    return g_bus != nullptr;
}

esp_err_t I2cBus::open(EspI2cRegisterBus& device, std::uint16_t address)
{
    ESP_RETURN_ON_FALSE(g_bus != nullptr, ESP_ERR_INVALID_STATE, TAG, "I2C bus is not initialized");
    return device.open(g_bus, address);
}

i2c_master_bus_handle_t I2cBus::handle()
{
    return g_bus;
}
} // namespace drivers
