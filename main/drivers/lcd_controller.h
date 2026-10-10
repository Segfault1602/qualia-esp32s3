#pragma once
/**
 * @brief Controller for the TL021WVC02CT LCD display
 *
 */

#include <esp_err.h>

#include "esp_lcd_panel_ops.h"

#define LCD_H_RES 480
#define LCD_V_RES 480

namespace drivers
{
/**
 * @brief The ST7701 RGB panel, configured over 3-wire SPI through the PCA9554.
 */
class Lcd
{
  public:
    Lcd() = delete;

    /**
     * @brief Requires Pca9554. Resets and configures the panel, clears both framebuffers to white, then turns the
     * backlight on.
     */
    static esp_err_t init();
    static bool is_initialized();

    /**
     * @brief Panel handle, or nullptr before init() succeeds.
     */
    static esp_lcd_panel_handle_t panel();

    static esp_err_t set_backlight(bool on);
};
} // namespace drivers
