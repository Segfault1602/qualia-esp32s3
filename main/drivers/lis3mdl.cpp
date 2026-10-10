#include "lis3mdl.h"

#include <esp_check.h>
#include <esp_log.h>

#include "esp_i2c_register_bus.h"
#include "i2c_bus.h"
#include "lis3mdl_device.h"

namespace
{
constexpr const char* TAG = "lis3mdl";

EspI2cRegisterBus g_bus;
imu::Lis3mdlDevice g_device(g_bus);
bool g_initialized = false;
} // namespace

namespace drivers
{
esp_err_t Lis3mdl::init()
{
    ESP_RETURN_ON_FALSE(!g_initialized, ESP_ERR_INVALID_STATE, TAG, "LIS3MDL is already initialized");
    ESP_RETURN_ON_FALSE(I2cBus::is_initialized(), ESP_ERR_INVALID_STATE, TAG, "I2C bus is not initialized");

    ESP_RETURN_ON_ERROR(I2cBus::open(g_bus, imu::Lis3mdlDevice::DEFAULT_ADDRESS), TAG, "Failed to add LIS3MDL");
    const auto result = g_device.init();
    if (!result)
    {
        const esp_err_t err = g_bus.to_esp_err(result.error());
        ESP_LOGW(TAG, "LIS3MDL unavailable: %s (%s)", imu::to_string(result.error()), esp_err_to_name(err));
        return err;
    }

    g_initialized = true;
    return ESP_OK;
}

bool Lis3mdl::is_initialized()
{
    return g_initialized;
}

std::expected<imu::Vec3, esp_err_t> Lis3mdl::read()
{
    if (!g_initialized)
    {
        return std::unexpected(ESP_ERR_INVALID_STATE);
    }
    return g_device.read().transform_error([](imu::Error error) { return g_bus.to_esp_err(error); });
}
} // namespace drivers
