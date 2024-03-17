#pragma once
/**
 * @file pca9554_controller.h
 * @brief driver code for the PCA9554 8-bit I/O expander
 * @version 0.1
 * @date 2024-02-16
 * ported from the Arduino_GFX library (https://github.com/moononournation/Arduino_GFX/)
 * https://www.ti.com/lit/ds/symlink/pca9554.pdf
 * @copyright Copyright (c) 2024
 *
 */

#include <stdbool.h>
#include <stdint.h>

#include <esp_err.h>
#include <esp_io_expander.h>

#define PCA9554_PIN_MODE_OUTPUT (1)
#define PCA9554_PIN_MODE_INPUT (0)
#define PCA9554_PIN_VALUE_HIGH (1)
#define PCA9554_PIN_VALUE_LOW (0)

#define PCA9554_TFT_SCK (IO_EXPANDER_PIN_NUM_0)
#define PCA9554_TFT_CS (IO_EXPANDER_PIN_NUM_1)
#define PCA9554_TFT_RESET (IO_EXPANDER_PIN_NUM_2)
#define PCA9554_TFT_BACKLIGHT (IO_EXPANDER_PIN_NUM_4)
#define PCA9554_BUTTON_UP (IO_EXPANDER_PIN_NUM_5)
#define PCA9554_BUTTON_DOWN (IO_EXPANDER_PIN_NUM_6)
#define PCA9554_TFT_MOSI (IO_EXPANDER_PIN_NUM_7)

extern esp_io_expander_handle_t g_io_expender;

/**
 * @brief Initialize the PCA9554 I2C expander
 *
 * @return esp_err_t
 */
esp_err_t pca9554_init(void);

bool pca9554_is_init(void);

/**
 * @brief Set the direction of the I/O pin
 *
 * @param pin The pin number
 * @param output true for output, false for input
 * @return esp_err_t
 */
esp_err_t pca9554_set_pin_mode(uint8_t pin, uint8_t mode);

/**
 * @brief Write to a I/O pin
 *
 * @param pin The pin number
 * @param value true for high, false for low
 * @return esp_err_t
 */
esp_err_t pca9554_digital_write(uint8_t pin, uint8_t value);

/**
 * @brief Enable or disable the TFT backlight
 *
 * @param enabled true to enable, false to disable
 * @return esp_err_t
 */
esp_err_t pca9554_enable_backlight(bool enabled);

esp_err_t pca9554_get_pin_state(uint8_t pin, uint8_t* value);

esp_err_t batch_operation(const uint8_t* operations, size_t len);