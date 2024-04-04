#include "renderer.h"

#include <freertos/FreeRTOS.h>

#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "freertos/semphr.h"
#include <box2d/box2d.h>
#include <esp_app_trace.h>
#include <esp_check.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/task.h>
#include <lvgl.h>

#include "lcd_controller.h"
#include "simulation.h"

#define LV_TICK_PERIOD_MS 1

namespace
{
constexpr char _TAG[] = "renderer";
SemaphoreHandle_t g_ui_mutex;

void lcd_flush_callback(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map)
{
    esp_lcd_panel_draw_bitmap(g_panel_handle, area->x1, area->y1, area->x2, area->y2, px_map);
    lv_disp_flush_ready(disp);
}

void lv_tick_task(void* args)
{
    lv_tick_inc(LV_TICK_PERIOD_MS);
}

esp_err_t init_renderer()
{
    g_ui_mutex = xSemaphoreCreateMutex();
    lv_init();

    void* buf1 = NULL;
    void* buf2 = NULL;
    esp_lcd_rgb_panel_get_frame_buffer(g_panel_handle, 2, &buf1, &buf2);

    lv_display_t* display = lv_display_create(LCD_H_RES, LCD_V_RES);
    if (!display)
    {
        ESP_LOGE(_TAG, "Failed to create display");
    }
    lv_display_set_default(display);
    lv_display_set_flush_cb(display, lcd_flush_callback);
    // lv_timer_del(display->refr_timer);
    // display->refr_timer = NULL;

    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_rotation(display, LV_DISPLAY_ROTATION_180);

    lv_display_set_buffers(display, buf1, buf2, LCD_H_RES * LCD_V_RES * sizeof(uint16_t), LV_DISPLAY_RENDER_MODE_FULL);

    /* Create and start a periodic timer interrupt to call lv_tick_inc */
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        // .arg = NULL,
        // .dispatch_method = ESP_TIMER_TASK,
        .name = "periodic_gui",
        // .skip_unhandled_events = true,
    };
    esp_timer_handle_t periodic_timer;
    ESP_RETURN_ON_ERROR(esp_timer_create(&periodic_timer_args, &periodic_timer), _TAG,
                        "Failed to create periodic timer");
    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000), _TAG,
                        "Failed to start periodic timer");

    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), LV_PART_MAIN);

    ESP_LOGI(_TAG, "LVGL initialized");
    return ESP_OK;
}

} // namespace

void simulate_task(void* pvParameters)
{
    ESP_LOGI(_TAG, "Simulation task started");

    while (1)
    {
        update_simulation();
        // if (xSemaphoreTake(g_ui_mutex, (TickType_t)10) == pdTRUE)
        // {
        //     draw_simulation();
        //     xSemaphoreGive(g_ui_mutex);
        // }
        vTaskDelay(1);
    }

    vTaskDelete(NULL);
}

void render_task(void* pvParameters)
{
    ESP_LOGI(_TAG, "Render task started");

    ESP_ERROR_CHECK(init_renderer());
    init_simulation();
    // xTaskCreatePinnedToCore(simulate_task, "simulate_task", 16384, NULL, 0, NULL, 1);

    while (1)
    {
        lv_timer_periodic_handler();
        update_simulation();
        // draw_simulation();
        vTaskDelay(1);
    }
    vTaskDelete(NULL);
}