#include "lcd_controller.h"

#include <algorithm>
#include <array>
#include <cstdint>

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_st7701.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_cache.h>
#include <esp_check.h>
#include <esp_lcd_panel_io_additions.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>

#include "lcd_init_commands.h"
#include "pca9554_controller.h"
#include "rgb565.h"

namespace
{
constexpr const char* TAG = "LCD";
constexpr std::uint32_t PIXEL_CLOCK_HZ = 16'000'000;
constexpr gpio_num_t PCLK = GPIO_NUM_1;
constexpr gpio_num_t DE = GPIO_NUM_2;
constexpr gpio_num_t HSYNC = GPIO_NUM_41;
constexpr gpio_num_t VSYNC = GPIO_NUM_42;
constexpr std::array DATA_PINS{
    GPIO_NUM_40, GPIO_NUM_39, GPIO_NUM_38, GPIO_NUM_0,  GPIO_NUM_45, GPIO_NUM_48, GPIO_NUM_47, GPIO_NUM_21,
    GPIO_NUM_14, GPIO_NUM_13, GPIO_NUM_12, GPIO_NUM_11, GPIO_NUM_10, GPIO_NUM_9,  GPIO_NUM_46, GPIO_NUM_3,
};

esp_lcd_panel_handle_t g_panel = nullptr;
bool g_initialized = false;

constexpr auto INIT_COMMANDS = [] {
    std::array<st7701_lcd_init_cmd_t, drivers::lcd::INIT_COMMANDS.size()> commands{};
    for (std::size_t i = 0; i < commands.size(); ++i)
    {
        const auto& source = drivers::lcd::INIT_COMMANDS[i];
        commands[i] = {source.command, source.data.data(), source.data.size(), source.delay_ms};
    }
    return commands;
}();

esp_err_t reset_panel()
{
    using drivers::Pca9554;
    ESP_RETURN_ON_ERROR(Pca9554::set_level(Pca9554::TFT_RESET, false), TAG, "Failed to assert display reset");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(Pca9554::set_level(Pca9554::TFT_RESET, true), TAG, "Failed to release display reset");
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_RETURN_ON_ERROR(Pca9554::set_level(Pca9554::TFT_CS | Pca9554::TFT_SCK | Pca9554::TFT_MOSI, true), TAG,
                        "Failed to initialize SPI lines");
    return ESP_OK;
}
} // namespace

namespace drivers
{
esp_err_t Lcd::init()
{
    ESP_RETURN_ON_FALSE(!g_initialized, ESP_ERR_INVALID_STATE, TAG, "LCD is already initialized");
    ESP_RETURN_ON_FALSE(Pca9554::is_initialized(), ESP_ERR_INVALID_STATE, TAG, "PCA9554 is not initialized");
    ESP_RETURN_ON_ERROR(reset_panel(), TAG, "Failed to reset panel");

    esp_lcd_rgb_panel_config_t rgb_config{};
    rgb_config.clk_src = LCD_CLK_SRC_PLL160M;
    rgb_config.dma_burst_size = 64;
    rgb_config.data_width = 16;
    rgb_config.in_color_format = LCD_COLOR_FMT_RGB565;
    rgb_config.out_color_format = LCD_COLOR_FMT_RGB565;
    rgb_config.num_fbs = 2;
    rgb_config.de_gpio_num = DE;
    rgb_config.pclk_gpio_num = PCLK;
    rgb_config.vsync_gpio_num = VSYNC;
    rgb_config.hsync_gpio_num = HSYNC;
    rgb_config.disp_gpio_num = GPIO_NUM_NC;
    std::ranges::copy(DATA_PINS, rgb_config.data_gpio_nums);

    rgb_config.timings.pclk_hz = PIXEL_CLOCK_HZ;
    rgb_config.timings.h_res = lcd::WIDTH;
    rgb_config.timings.v_res = lcd::HEIGHT;
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
        .cs_expander_pin = Pca9554::TFT_CS,
        .scl_io_type = IO_TYPE_EXPANDER,
        .scl_expander_pin = Pca9554::TFT_SCK,
        .sda_io_type = IO_TYPE_EXPANDER,
        .sda_expander_pin = Pca9554::TFT_MOSI,
        .io_expander = Pca9554::handle(),
    };

    esp_lcd_panel_io_3wire_spi_config_t io_config = ST7701_PANEL_IO_3WIRE_SPI_CONFIG(line_config, 0);
    esp_lcd_panel_io_handle_t io_handle = nullptr;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_3wire_spi(&io_config, &io_handle), TAG,
                        "Failed to initialize LCD panel IO");

    st7701_vendor_config_t vendor_config{};
    vendor_config.rgb_config = &rgb_config;
    vendor_config.init_cmds = INIT_COMMANDS.data();
    vendor_config.init_cmds_size = INIT_COMMANDS.size();
    vendor_config.flags.auto_del_panel_io = 1;
    esp_lcd_panel_dev_config_t panel_config{};
    panel_config.reset_gpio_num = GPIO_NUM_NC;
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_config.bits_per_pixel = 16;
    panel_config.vendor_config = &vendor_config;

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7701(io_handle, &panel_config, &g_panel), TAG,
                        "Failed to create ST7701 panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(g_panel), TAG, "Failed to reset panel");
    std::array<void*, 2> framebuffers{};
    ESP_RETURN_ON_ERROR(esp_lcd_rgb_panel_get_frame_buffer(g_panel, 2, &framebuffers[0], &framebuffers[1]), TAG,
                        "Failed to get framebuffers");
    for (void* fb : framebuffers)
    {
        ESP_RETURN_ON_FALSE(fb != nullptr, ESP_ERR_INVALID_STATE, TAG, "Missing LCD framebuffer");
        rgb565::fill({static_cast<std::uint16_t*>(fb), lcd::PIXEL_COUNT}, 0xFFFF);
        ESP_RETURN_ON_ERROR(esp_cache_msync(fb, lcd::FRAMEBUFFER_BYTES, ESP_CACHE_MSYNC_FLAG_DIR_C2M), TAG,
                            "Failed to flush framebuffer");
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(g_panel), TAG, "Failed to initialize panel");
    ESP_RETURN_ON_ERROR(set_backlight(true), TAG, "Failed to enable backlight");

    g_initialized = true;
    ESP_LOGI(TAG, "LCD controller initialized");
    return ESP_OK;
}

bool Lcd::is_initialized()
{
    return g_initialized;
}

esp_lcd_panel_handle_t Lcd::panel()
{
    return g_panel;
}

esp_err_t Lcd::set_backlight(bool on)
{
    ESP_RETURN_ON_FALSE(Pca9554::is_initialized(), ESP_ERR_INVALID_STATE, TAG, "PCA9554 is not initialized");
    return Pca9554::set_level(Pca9554::TFT_BACKLIGHT, on);
}
} // namespace drivers
