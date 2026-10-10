#include "pca9554_controller.h"

#include <memory>
#include <type_traits>

#include <esp_check.h>
#include <esp_io_expander_tca9554.h>

#include "esp_i2c_register_bus.h"
#include "i2c_bus.h"

namespace
{
constexpr const char* TAG = "PCA9554";
constexpr std::uint16_t ADDRESS = 0x3F;
constexpr std::uint8_t INPUT_PORT_REG = 0x00;
constexpr std::uint8_t INVERSION_PORT_REG = 0x02;

esp_io_expander_handle_t g_expander = nullptr;
// Our own device at the expander's address, for registers the component does not cache (polarity, input port).
// Its transfers use the board's 20 ms timeout rather than the component's 1000 ms.
EspI2cRegisterBus g_registers;
bool g_initialized = false;

void delete_expander(esp_io_expander_handle_t expander)
{
    ESP_ERROR_CHECK(esp_io_expander_del(expander));
}
} // namespace

namespace drivers
{
esp_err_t Pca9554::init()
{
    ESP_RETURN_ON_FALSE(!g_initialized, ESP_ERR_INVALID_STATE, TAG, "PCA9554 is already initialized");
    ESP_RETURN_ON_FALSE(I2cBus::is_initialized(), ESP_ERR_INVALID_STATE, TAG, "I2C bus is not initialized");

    ESP_RETURN_ON_ERROR(I2cBus::open(g_registers, ADDRESS), TAG, "Failed to add register device");
    const std::uint8_t no_inversion = 0;
    ESP_RETURN_ON_ERROR(static_cast<esp_err_t>(g_registers.write(INVERSION_PORT_REG, {&no_inversion, 1})), TAG,
                        "Failed to clear input inversion");

    esp_io_expander_handle_t handle = nullptr;
    const esp_err_t created = esp_io_expander_new_i2c_tca9554(I2cBus::handle(), ADDRESS, &handle);
    std::unique_ptr<std::remove_pointer_t<esp_io_expander_handle_t>, decltype(&delete_expander)> expander{
        handle, delete_expander};
    ESP_RETURN_ON_ERROR(created, TAG, "Failed to create IO expander");
    ESP_RETURN_ON_ERROR(esp_io_expander_set_dir(expander.get(), 0xFF, IO_EXPANDER_OUTPUT), TAG,
                        "Failed to configure outputs");
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(expander.get(), 0xFF, 0), TAG, "Failed to clear outputs");
    ESP_RETURN_ON_ERROR(esp_io_expander_set_dir(expander.get(), BUTTON_UP | BUTTON_DOWN, IO_EXPANDER_INPUT), TAG,
                        "Failed to configure button inputs");
    ESP_RETURN_ON_ERROR(esp_io_expander_print_state(expander.get()), TAG, "Failed to read expander state");

    g_expander = expander.release();
    g_initialized = true;
    return ESP_OK;
}

bool Pca9554::is_initialized()
{
    return g_initialized;
}

esp_io_expander_handle_t Pca9554::handle()
{
    return g_expander;
}

esp_err_t Pca9554::set_direction(std::uint32_t pins, esp_io_expander_dir_t direction)
{
    ESP_RETURN_ON_ERROR(esp_io_expander_set_dir(g_expander, pins, direction), TAG, "Failed to set pin direction");
    return ESP_OK;
}

esp_err_t Pca9554::set_level(std::uint32_t pins, bool high)
{
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(g_expander, pins, high ? 1 : 0), TAG, "Failed to set pin level");
    return ESP_OK;
}

std::expected<bool, esp_err_t> Pca9554::get_level(std::uint32_t pin)
{
    if (!g_initialized)
    {
        return std::unexpected(ESP_ERR_INVALID_STATE);
    }
    std::uint8_t input_port = 0;
    const esp_err_t err = static_cast<esp_err_t>(g_registers.read(INPUT_PORT_REG, {&input_port, 1}));
    if (err != ESP_OK)
    {
        return std::unexpected(err);
    }
    return (input_port & pin) != 0;
}
} // namespace drivers
