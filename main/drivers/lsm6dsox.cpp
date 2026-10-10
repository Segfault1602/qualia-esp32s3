#include "lsm6dsox.h"

#include <esp_check.h>
#include <esp_log.h>

#include "esp_i2c_register_bus.h"
#include "i2c_bus.h"
#include "lsm6dsox_device.h"

namespace
{
constexpr const char* TAG = "lsm6dsox";

EspI2cRegisterBus g_bus;
imu::Lsm6dsoxDevice g_device(g_bus);
bool g_initialized = false;
} // namespace

namespace drivers
{
esp_err_t Lsm6dsox::init()
{
    ESP_RETURN_ON_FALSE(!g_initialized, ESP_ERR_INVALID_STATE, TAG, "LSM6DSOX is already initialized");
    ESP_RETURN_ON_FALSE(I2cBus::is_initialized(), ESP_ERR_INVALID_STATE, TAG, "I2C bus is not initialized");

    ESP_RETURN_ON_ERROR(I2cBus::open(g_bus, imu::Lsm6dsoxDevice::DEFAULT_ADDRESS), TAG, "Failed to add LSM6DSOX");
    const auto result = g_device.init();
    if (!result)
    {
        const esp_err_t err = g_bus.to_esp_err(result.error());
        ESP_LOGW(TAG, "LSM6DSOX unavailable: %s (%s)", imu::to_string(result.error()), esp_err_to_name(err));
        return err;
    }

    g_initialized = true;
    return ESP_OK;
}

bool Lsm6dsox::is_initialized()
{
    return g_initialized;
}

std::expected<imu::MotionSample, esp_err_t> Lsm6dsox::read()
{
    if (!g_initialized)
    {
        return std::unexpected(ESP_ERR_INVALID_STATE);
    }
    return g_device.read().transform_error([](imu::Error error) { return g_bus.to_esp_err(error); });
}
} // namespace drivers
