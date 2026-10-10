#pragma once
/**
 * @file pca9554_controller.h
 * @brief Driver for the PCA9554 8-bit I/O expander
 * https://www.ti.com/lit/ds/symlink/pca9554.pdf
 */

#include <cstdint>
#include <expected>

#include <esp_err.h>
#include <esp_io_expander.h>

namespace drivers
{
/**
 * @brief The board's PCA9554 I/O expander on the shared I2C bus.
 *
 * Pin arguments are esp_io_expander pin masks. The button pins are inputs; every other pin starts as a low output.
 *
 * set_level() and set_direction() update a cached register and then write it, so they are not safe to call from
 * two tasks at once, and they use the component's fixed 1000 ms transfer timeout. After startup, only one task may
 * change outputs (the backlight). get_level() reads the input register through the driver's own device, with the
 * board's I2C_MASTER_TIMEOUT_MS, and is safe from any task.
 */
class Pca9554
{
  public:
    static constexpr esp_io_expander_pin_num_t TFT_SCK = IO_EXPANDER_PIN_NUM_0;
    static constexpr esp_io_expander_pin_num_t TFT_CS = IO_EXPANDER_PIN_NUM_1;
    static constexpr esp_io_expander_pin_num_t TFT_RESET = IO_EXPANDER_PIN_NUM_2;
    static constexpr esp_io_expander_pin_num_t TFT_BACKLIGHT = IO_EXPANDER_PIN_NUM_4;
    static constexpr esp_io_expander_pin_num_t BUTTON_UP = IO_EXPANDER_PIN_NUM_5;
    static constexpr esp_io_expander_pin_num_t BUTTON_DOWN = IO_EXPANDER_PIN_NUM_6;
    static constexpr esp_io_expander_pin_num_t TFT_MOSI = IO_EXPANDER_PIN_NUM_7;

    Pca9554() = delete;

    /**
     * @brief Requires I2cBus.
     */
    static esp_err_t init();
    static bool is_initialized();

    /**
     * @brief Expander handle, or nullptr before init() succeeds.
     */
    static esp_io_expander_handle_t handle();

    static esp_err_t set_direction(std::uint32_t pins, esp_io_expander_dir_t direction);
    static esp_err_t set_level(std::uint32_t pins, bool high);

    /**
     * @brief Input level of one pin. Callers log failures; this does not.
     */
    static std::expected<bool, esp_err_t> get_level(std::uint32_t pin);
};
} // namespace drivers
