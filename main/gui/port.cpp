#include "gui/port.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>

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

namespace
{
constexpr const char* TAG = "gui";
constexpr std::chrono::milliseconds LV_TICK_PERIOD{1};
constexpr TickType_t FRAMEBUFFER_RELEASE_TIMEOUT = pdMS_TO_TICKS(1000);

std::atomic<std::uint32_t> g_vsync_count{0};
std::atomic<std::int64_t> g_vsync_time_us{0};
std::atomic<std::int64_t> g_frame_period_us{0};
std::uint32_t g_release_vsync_count = 0;

// Swapping this close to the end of a frame can lose the race with the LCD DMA, which may then scan the old
// framebuffer for one more frame.
constexpr std::int64_t LATE_SWAP_MARGIN_US = 2'500;

bool IRAM_ATTR vsync_complete(esp_lcd_panel_handle_t, const esp_lcd_rgb_panel_event_data_t*, void* user_ctx)
{
    const std::int64_t now = esp_timer_get_time();
    g_frame_period_us.store(now - g_vsync_time_us.load(std::memory_order_relaxed), std::memory_order_relaxed);
    g_vsync_time_us.store(now, std::memory_order_relaxed);
    g_vsync_count.fetch_add(1, std::memory_order_relaxed);
    BaseType_t need_yield = pdFALSE;
    vTaskNotifyGiveFromISR(static_cast<TaskHandle_t>(user_ctx), &need_yield);
    return need_yield == pdTRUE;
}

// The previously scanned framebuffer is free once the DMA has moved on to the presented one.
void lcd_flush_wait_callback(lv_display_t*)
{
    const TickType_t start = xTaskGetTickCount();
    while (static_cast<std::int32_t>(g_vsync_count.load(std::memory_order_relaxed) - g_release_vsync_count) < 0)
    {
        const TickType_t elapsed = xTaskGetTickCount() - start;
        if (elapsed >= FRAMEBUFFER_RELEASE_TIMEOUT)
        {
            ESP_LOGE(TAG, "Timed out waiting for LCD framebuffer release");
            ESP_ERROR_CHECK(ESP_ERR_TIMEOUT);
        }
        ulTaskNotifyTake(pdTRUE, FRAMEBUFFER_RELEASE_TIMEOUT - elapsed);
    }
}

void lcd_flush_callback(lv_display_t* disp, const lv_area_t*, std::uint8_t* px_map)
{
    if (!lv_display_flush_is_last(disp))
    {
        lv_display_flush_ready(disp);
        return;
    }
    // DIRECT mode renders and synchronizes areas in place, so present the whole buffer.
    ESP_ERROR_CHECK(
        esp_lcd_panel_draw_bitmap(drivers::Lcd::panel(), 0, 0, drivers::lcd::WIDTH, drivers::lcd::HEIGHT, px_map));
    // Read the timestamp before the count so a VSYNC in between can only make the swap look late.
    const std::int64_t vsync_time_us = g_vsync_time_us.load(std::memory_order_relaxed);
    const std::uint32_t vsync_count = g_vsync_count.load(std::memory_order_relaxed);
    const std::int64_t phase_us = esp_timer_get_time() - vsync_time_us;
    const bool late = phase_us > g_frame_period_us.load(std::memory_order_relaxed) - LATE_SWAP_MARGIN_US;
    g_release_vsync_count = vsync_count + (late ? 2 : 1);
}

void lv_tick_task(void*)
{
    lv_tick_inc(LV_TICK_PERIOD.count());
}

} // namespace

namespace gui
{
esp_err_t init()
{
    lv_init();

    std::array<void*, 2> framebuffers{};
    ESP_RETURN_ON_ERROR(
        esp_lcd_rgb_panel_get_frame_buffer(drivers::Lcd::panel(), 2, &framebuffers[0], &framebuffers[1]), TAG,
        "Failed to get framebuffers");
    ESP_RETURN_ON_FALSE(framebuffers[0] != nullptr && framebuffers[1] != nullptr && framebuffers[0] != framebuffers[1],
                        ESP_ERR_INVALID_STATE, TAG, "Invalid LCD framebuffers");

    lv_display_t* display = lv_display_create(drivers::lcd::WIDTH, drivers::lcd::HEIGHT);
    ESP_RETURN_ON_FALSE(display != nullptr, ESP_ERR_NO_MEM, TAG, "Failed to create display");
    lv_display_set_default(display);
    lv_display_set_flush_cb(display, lcd_flush_callback);
    lv_display_set_flush_wait_cb(display, lcd_flush_wait_callback);

    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    // The RGB driver initially scans framebuffer 0, so render into framebuffer 1 first.
    lv_display_set_buffers(display, framebuffers[1], framebuffers[0], drivers::lcd::FRAMEBUFFER_BYTES,
                           LV_DISPLAY_RENDER_MODE_DIRECT);
    esp_lcd_rgb_panel_event_callbacks_t callbacks{};
    callbacks.on_vsync = vsync_complete;
    ESP_RETURN_ON_ERROR(
        esp_lcd_rgb_panel_register_event_callbacks(drivers::Lcd::panel(), &callbacks, xTaskGetCurrentTaskHandle()), TAG,
        "Failed to register VSYNC callback");

    esp_timer_create_args_t periodic_timer_args{};
    periodic_timer_args.callback = &lv_tick_task;
    periodic_timer_args.dispatch_method = ESP_TIMER_TASK;
    periodic_timer_args.name = "periodic_gui";
    esp_timer_handle_t periodic_timer;
    ESP_RETURN_ON_ERROR(esp_timer_create(&periodic_timer_args, &periodic_timer), TAG,
                        "Failed to create periodic timer");
    const auto tick_period_us = std::chrono::duration_cast<std::chrono::microseconds>(LV_TICK_PERIOD);
    const esp_err_t timer_result = esp_timer_start_periodic(periodic_timer, tick_period_us.count());
    if (timer_result != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start periodic timer: %s", esp_err_to_name(timer_result));
        ESP_ERROR_CHECK(esp_timer_delete(periodic_timer));
        return timer_result;
    }

    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), LV_PART_MAIN);
    // The ball's canvas can overhang the panel edge for a frame on hard impacts; a scrollable screen would then
    // flash its scrollbars along the right and bottom edges.
    lv_obj_set_scrollable(lv_screen_active(), false);

    ESP_LOGI(TAG, "LVGL initialized");
    return ESP_OK;
}

void run_timers()
{
    lv_timer_periodic_handler();
}
} // namespace gui
