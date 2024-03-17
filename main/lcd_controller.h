#pragma once
/**
 * @brief Controller for the TL021WVC02CT LCD display
 *
 */

#include <esp_err.h>

#include "esp_intr_alloc.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_pm.h"
#include "esp_private/gdma.h"
#include "hal/dma_types.h"

#include "hal/lcd_hal.h"
#include "hal/lcd_ll.h"

#include "esp32s3/rom/cache.h"

#define LCD_H_RES 480
#define LCD_V_RES 480

extern esp_lcd_panel_handle_t g_panel_handle;

esp_err_t lcd_controller_init();