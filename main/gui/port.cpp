#include "gui/port.h"

#include <atomic>

#include <freertos/FreeRTOS.h>

#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include <esp_check.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/task.h>
#include <lvgl.h>

#include "lcd_controller.h"

#define LV_TICK_PERIOD_MS 1

namespace
{
constexpr char _TAG[] = "gui";
constexpr TickType_t FRAMEBUFFER_RELEASE_TIMEOUT = pdMS_TO_TICKS(1000);

std::atomic<uint32_t> g_vsync_count{0};
std::atomic<int64_t> g_vsync_time_us{0};
std::atomic<int64_t> g_frame_period_us{0};
uint32_t g_release_vsync_count = 0;

// Swapping this close to the end of a frame can lose the race with the LCD DMA, which may then scan the old
// framebuffer for one more frame.
constexpr int64_t LATE_SWAP_MARGIN_US = 2'500;

bool IRAM_ATTR vsync_complete(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t* event,
                              void* user_ctx)
{
    const int64_t now = esp_timer_get_time();
    g_frame_period_us.store(now - g_vsync_time_us.load(std::memory_order_relaxed), std::memory_order_relaxed);
    g_vsync_time_us.store(now, std::memory_order_relaxed);
    g_vsync_count.fetch_add(1, std::memory_order_relaxed);
    BaseType_t need_yield = pdFALSE;
    vTaskNotifyGiveFromISR(static_cast<TaskHandle_t>(user_ctx), &need_yield);
    return need_yield == pdTRUE;
}

// The previously scanned framebuffer is free once the DMA has moved on to the presented one.
void lcd_flush_wait_callback(lv_display_t* disp)
{
    const TickType_t start = xTaskGetTickCount();
    while (static_cast<int32_t>(g_vsync_count.load(std::memory_order_relaxed) - g_release_vsync_count) < 0)
    {
        const TickType_t elapsed = xTaskGetTickCount() - start;
        if (elapsed >= FRAMEBUFFER_RELEASE_TIMEOUT)
        {
            ESP_LOGE(_TAG, "Timed out waiting for LCD framebuffer release");
            ESP_ERROR_CHECK(ESP_ERR_TIMEOUT);
        }
        ulTaskNotifyTake(pdTRUE, FRAMEBUFFER_RELEASE_TIMEOUT - elapsed);
    }
}

void lcd_flush_callback(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map)
{
    if (!lv_display_flush_is_last(disp))
    {
        lv_display_flush_ready(disp);
        return;
    }
    // DIRECT mode renders and synchronizes areas in place, so present the whole buffer.
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(drivers::Lcd::panel(), 0, 0, LCD_H_RES, LCD_V_RES, px_map));
    // Read the timestamp before the count so a VSYNC in between can only make the swap look late.
    const int64_t vsync_time_us = g_vsync_time_us.load(std::memory_order_relaxed);
    const uint32_t vsync_count = g_vsync_count.load(std::memory_order_relaxed);
    const int64_t phase_us = esp_timer_get_time() - vsync_time_us;
    const bool late = phase_us > g_frame_period_us.load(std::memory_order_relaxed) - LATE_SWAP_MARGIN_US;
    g_release_vsync_count = vsync_count + (late ? 2 : 1);
}

void lv_tick_task(void* args)
{
    lv_tick_inc(LV_TICK_PERIOD_MS);
}

} // namespace

namespace gui
{
esp_err_t init()
{
    lv_init();

    void* buf1 = NULL;
    void* buf2 = NULL;
    ESP_RETURN_ON_ERROR(esp_lcd_rgb_panel_get_frame_buffer(drivers::Lcd::panel(), 2, &buf1, &buf2), _TAG,
                        "Failed to get framebuffers");
    ESP_RETURN_ON_FALSE(buf1 != nullptr && buf2 != nullptr && buf1 != buf2, ESP_ERR_INVALID_STATE, _TAG,
                        "Invalid LCD framebuffers");

    lv_display_t* display = lv_display_create(LCD_H_RES, LCD_V_RES);
    ESP_RETURN_ON_FALSE(display != nullptr, ESP_ERR_NO_MEM, _TAG, "Failed to create display");
    lv_display_set_default(display);
    lv_display_set_flush_cb(display, lcd_flush_callback);
    lv_display_set_flush_wait_cb(display, lcd_flush_wait_callback);

    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    // The RGB driver initially scans framebuffer 0, so render into framebuffer 1 first.
    lv_display_set_buffers(display, buf2, buf1, LCD_H_RES * LCD_V_RES * sizeof(uint16_t), LV_DISPLAY_RENDER_MODE_DIRECT);
    esp_lcd_rgb_panel_event_callbacks_t callbacks{};
    callbacks.on_vsync = vsync_complete;
    ESP_RETURN_ON_ERROR(esp_lcd_rgb_panel_register_event_callbacks(drivers::Lcd::panel(), &callbacks,
                        xTaskGetCurrentTaskHandle()), _TAG, "Failed to register VSYNC callback");

    esp_timer_create_args_t periodic_timer_args{};
    periodic_timer_args.callback = &lv_tick_task;
    periodic_timer_args.dispatch_method = ESP_TIMER_TASK;
    periodic_timer_args.name = "periodic_gui";
    esp_timer_handle_t periodic_timer;
    ESP_RETURN_ON_ERROR(esp_timer_create(&periodic_timer_args, &periodic_timer), _TAG,
                        "Failed to create periodic timer");
    const esp_err_t timer_result = esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000);
    if (timer_result != ESP_OK)
    {
        ESP_LOGE(_TAG, "Failed to start periodic timer: %s", esp_err_to_name(timer_result));
        ESP_ERROR_CHECK(esp_timer_delete(periodic_timer));
        return timer_result;
    }

    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), LV_PART_MAIN);
    // The ball's canvas can overhang the panel edge for a frame on hard impacts; a scrollable screen would then
    // flash its scrollbars along the right and bottom edges.
    lv_obj_set_scrollable(lv_screen_active(), false);

    ESP_LOGI(_TAG, "LVGL initialized");
    return ESP_OK;
}

void run_timers()
{
    lv_timer_periodic_handler();
}
} // namespace gui
