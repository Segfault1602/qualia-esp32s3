#include "lcd_controller.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_st7701.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_check.h>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_io_additions.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <math.h>
#include <stdint.h>

#include "init_operations.h"
#include "pca9554_controller.h"

#define LCDTAG "LCD"

#define LCD_PIXEL_CLOCK_HZ (16000000L)

#define LCD_PIN_NUM_PCLK 1
#define LCD_PIN_NUM_DE 2
#define LCD_PIN_NUM_HSYNC 41
#define LCD_PIN_NUM_VSYNC 42

#define LCD_PIN_NUM_B0 40 // B0
#define LCD_PIN_NUM_B1 39 // B1
#define LCD_PIN_NUM_B2 38 // B2
#define LCD_PIN_NUM_B3 0  // B3
#define LCD_PIN_NUM_B4 45 // B4
#define LCD_PIN_NUM_G0 48 // G0
#define LCD_PIN_NUM_G1 47 // G1
#define LCD_PIN_NUM_G2 21 // G2
#define LCD_PIN_NUM_G3 14 // G3
#define LCD_PIN_NUM_G4 13 // G4
#define LCD_PIN_NUM_G5 12 // G5
#define LCD_PIN_NUM_R0 11 // R0
#define LCD_PIN_NUM_R1 10 // R1
#define LCD_PIN_NUM_R2 9  // R2
#define LCD_PIN_NUM_R3 46 // R3
#define LCD_PIN_NUM_R4 3  // R4

esp_lcd_panel_handle_t g_panel_handle;

static const st7701_lcd_init_cmd_t TL021WVC02_init_cmds[] = {
    //  {cmd, { data }, data_size, delay_ms}
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
    {0xC0, (uint8_t[]){0x3B, 0x00}, 2, 0},
    {0xC1, (uint8_t[]){0x0B, 0x02}, 2, 0},
    {0xC2, (uint8_t[]){0x00, 0x02}, 2, 0},
    {0xCC, (uint8_t[]){0x10}, 1, 0},
    {0xCD, (uint8_t[]){0x08}, 1, 0},
    {0xB0, (uint8_t[]){0x02, 0x13, 0x1B, 0x0D, 0x10, 0x05, 0x08, 0x07, 0x07, 0x24, 0x04, 0x11, 0x0E, 0x2C, 0x33, 0x1D},
     16, 0},
    {0xB1, (uint8_t[]){0x05, 0x13, 0x1B, 0x0D, 0x11, 0x05, 0x08, 0x07, 0x07, 0x24, 0x04, 0x11, 0x0E, 0x2C, 0x33, 0x1D},
     16, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0},
    {0xB0, (uint8_t[]){0x5D}, 1, 0},
    {0xB1, (uint8_t[]){0x43}, 1, 0},
    {0xB2, (uint8_t[]){0x81}, 1, 0},
    {0xB3, (uint8_t[]){0x80}, 1, 0},
    {0xB5, (uint8_t[]){0x43}, 1, 0},
    {0xB7, (uint8_t[]){0x85}, 1, 0},
    {0xB8, (uint8_t[]){0x20}, 1, 0},
    {0xC1, (uint8_t[]){0x78}, 1, 0},
    {0xC2, (uint8_t[]){0x78}, 1, 0},
    {0xD0, (uint8_t[]){0x88}, 1, 100},
    {0xE0, (uint8_t[]){0x00, 0x00, 0x02}, 3, 0},
    {0xE1, (uint8_t[]){0x03, 0xA0, 0x00, 0x00, 0x04, 0xA0, 0x00, 0x00, 0x00, 0x20, 0x20}, 11, 0},
    {0xE2, (uint8_t[]){0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 13, 0},
    {0xE3, (uint8_t[]){0x00, 0x00, 0x11, 0x00}, 4, 0},
    {0xE4, (uint8_t[]){0x22, 0x00}, 2, 0},
    {0xE5, (uint8_t[]){0x05, 0xEC, 0xA0, 0xA0, 0x07, 0xEE, 0xA0, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
     16, 0},
    {0xE6, (uint8_t[]){0x00, 0x00, 0x11, 0x00}, 4, 0},
    {0xE7, (uint8_t[]){0x22, 0x00}, 2, 0},
    {0xE8, (uint8_t[]){0x06, 0xED, 0xA0, 0xA0, 0x08, 0xEF, 0xA0, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
     16, 0},
    {0xEB, (uint8_t[]){0x00, 0x00, 0x40, 0x40, 0x00, 0x00, 0x00}, 7, 0},
    {0xED, (uint8_t[]){0xFF, 0xFF, 0xFF, 0xBA, 0x0A, 0xBF, 0x45, 0xFF, 0xFF, 0x54, 0xFB, 0xA0, 0xAB, 0xFF, 0xFF, 0xFF},
     16, 0},
    {0xEF, (uint8_t[]){0x10, 0x0D, 0x04, 0x08, 0x3F, 0x1F}, 6, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    {0xEF, (uint8_t[]){0x08}, 1, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x00}, 5, 0},
    {0x36, (uint8_t[]){0x00}, 1, 0},
    {0x3A, (uint8_t[]){0x60}, 1, 0},
    {0x11, (uint8_t[]){0x00}, 0, 120},
    {0x29, (uint8_t[]){0x00}, 0, 0},
};

esp_err_t lcd_controller_init()
{
    esp_lcd_rgb_panel_config_t rgb_config;
    memset(&rgb_config, 0, sizeof(esp_lcd_rgb_panel_config_t));
    rgb_config.clk_src = LCD_CLK_SRC_PLL160M;
    rgb_config.psram_trans_align = 64;
    rgb_config.data_width = 16;
    rgb_config.sram_trans_align = 8;
    rgb_config.num_fbs = 2;
    rgb_config.de_gpio_num = LCD_PIN_NUM_DE;
    rgb_config.pclk_gpio_num = LCD_PIN_NUM_PCLK;
    rgb_config.vsync_gpio_num = LCD_PIN_NUM_VSYNC;
    rgb_config.hsync_gpio_num = LCD_PIN_NUM_HSYNC;
    rgb_config.disp_gpio_num = -1;
    rgb_config.data_gpio_nums[0] = LCD_PIN_NUM_B0;
    rgb_config.data_gpio_nums[1] = LCD_PIN_NUM_B1;
    rgb_config.data_gpio_nums[2] = LCD_PIN_NUM_B2;
    rgb_config.data_gpio_nums[3] = LCD_PIN_NUM_B3;
    rgb_config.data_gpio_nums[4] = LCD_PIN_NUM_B4;
    rgb_config.data_gpio_nums[5] = LCD_PIN_NUM_G0;
    rgb_config.data_gpio_nums[6] = LCD_PIN_NUM_G1;
    rgb_config.data_gpio_nums[7] = LCD_PIN_NUM_G2;
    rgb_config.data_gpio_nums[8] = LCD_PIN_NUM_G3;
    rgb_config.data_gpio_nums[9] = LCD_PIN_NUM_G4;
    rgb_config.data_gpio_nums[10] = LCD_PIN_NUM_G5;
    rgb_config.data_gpio_nums[11] = LCD_PIN_NUM_R0;
    rgb_config.data_gpio_nums[12] = LCD_PIN_NUM_R1;
    rgb_config.data_gpio_nums[13] = LCD_PIN_NUM_R2;
    rgb_config.data_gpio_nums[14] = LCD_PIN_NUM_R3;
    rgb_config.data_gpio_nums[15] = LCD_PIN_NUM_R4;

    // rgb_config.data_gpio_nums = {LCD_PIN_NUM_B0, LCD_PIN_NUM_B1, LCD_PIN_NUM_B2, LCD_PIN_NUM_B3,
    //                              LCD_PIN_NUM_B4, LCD_PIN_NUM_G0, LCD_PIN_NUM_G1, LCD_PIN_NUM_G2,
    //                              LCD_PIN_NUM_G3, LCD_PIN_NUM_G4, LCD_PIN_NUM_G5, LCD_PIN_NUM_R0,
    //                              LCD_PIN_NUM_R1, LCD_PIN_NUM_R2, LCD_PIN_NUM_R3, LCD_PIN_NUM_R4};
    rgb_config.timings.pclk_hz = LCD_PIXEL_CLOCK_HZ;
    rgb_config.timings.h_res = LCD_H_RES;
    rgb_config.timings.v_res = LCD_V_RES;
    rgb_config.timings.hsync_pulse_width = 2;
    rgb_config.timings.hsync_back_porch = 44;
    rgb_config.timings.hsync_front_porch = 50;
    rgb_config.timings.vsync_pulse_width = 2;
    rgb_config.timings.vsync_back_porch = 18;
    rgb_config.timings.vsync_front_porch = 16;
    rgb_config.timings.flags.hsync_idle_low = 0;
    rgb_config.timings.flags.vsync_idle_low = 0;
    rgb_config.timings.flags.de_idle_high = 0;
    rgb_config.timings.flags.pclk_active_neg = 0;
    rgb_config.timings.flags.pclk_idle_high = 0;
    rgb_config.flags.fb_in_psram = 1;
    rgb_config.flags.disp_active_low = 0;
    rgb_config.flags.refresh_on_demand = 0;

    spi_line_config_t line_config = {
        .cs_io_type = IO_TYPE_EXPANDER,
        .cs_gpio_num = PCA9554_TFT_CS,
        .scl_io_type = IO_TYPE_EXPANDER,
        .scl_gpio_num = PCA9554_TFT_SCK,
        .sda_io_type = IO_TYPE_EXPANDER,
        .sda_gpio_num = PCA9554_TFT_MOSI,
        .io_expander = g_io_expender, // Set to NULL if not using IO expander
    };

    esp_lcd_panel_io_3wire_spi_config_t io_config = ST7701_PANEL_IO_3WIRE_SPI_CONFIG(line_config, 0);
    esp_lcd_panel_io_handle_t io_handle = NULL;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_3wire_spi(&io_config, &io_handle), LCDTAG,
                        "Failed to initialize LCD panel IO");

    st7701_vendor_config_t vendor_config = {
        .rgb_config = &rgb_config,
        .init_cmds = TL021WVC02_init_cmds, // Uncomment these line if use custom initialization commands
        .init_cmds_size = sizeof(TL021WVC02_init_cmds) / sizeof(st7701_lcd_init_cmd_t),
        .flags =
            {
                .auto_del_panel_io = 1, /**
                                         * Set to 1 if panel IO is no longer needed after LCD initialization.
                                         * If the panel IO pins are sharing other pins of the RGB interface to save
                                         * GPIOs, Please set it to 1 to release the pins.
                                         */
            },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1,                       // Set to -1 if not use
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB, // Implemented by LCD command `36h`
        .bits_per_pixel = 16,                       // Implemented by LCD command `3Ah` (16/18/24)
        .vendor_config = &vendor_config,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_st7701(io_handle, &panel_config, &g_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(g_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(g_panel_handle));

    // Fill the screen white
    void* fb;
    esp_lcd_rgb_panel_get_frame_buffer(g_panel_handle, 1, &fb);

    uint32_t color = 0xFFFF;
    for (int i = 0; i < 480 * 480; i++)
    {
        ((uint32_t*)fb)[i] = color;
    }

    ESP_ERROR_CHECK(g_panel_handle->draw_bitmap(g_panel_handle, 0, 0, 480, 480, fb));

    ESP_LOGI(__func__, "LCD controller initialized");
    return ESP_OK;
}