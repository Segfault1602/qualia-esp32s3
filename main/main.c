#include <stdio.h>

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
// #include <lv_conf.h>
#include <lvgl.h>
#include <src/display/lv_display_private.h>
#include <math.h>

#include "buttons.h"
#include "defines.h"
#include "init_operations.h"
#include "lcd_controller.h"
#include "pca9554_controller.h"

#define MIDI_DEVICE 0
#if MIDI_DEVICE
#include "midi_usb.h"
#endif // MIDI_DEVICE

#define LV_TICK_PERIOD_MS 1

static void button_up_cb(uint8_t button)
{
    if (button == BUTTON_UP)
    {
        ESP_LOGI(__func__, "Button up pressed");
        ESP_ERROR_CHECK(pca9554_enable_backlight(true));
    }
    else if (button == BUTTON_DOWN)
    {
        ESP_LOGI(__func__, "Button down pressed");
        ESP_ERROR_CHECK(pca9554_enable_backlight(false));
    }
}

static uint32_t highwatermark = 0;
static uint32_t lowwatermark = 0xFFFFFFFF;
static uint32_t current_delay = 0;
static uint32_t last_time = 0;

static void lcd_flush_callback(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map)
{
    uint32_t current_time = esp_timer_get_time();
    current_delay = current_time - last_time;
    if (current_delay > highwatermark)
    {
        highwatermark = current_delay;
    }
    if (current_delay < lowwatermark)
    {
        lowwatermark = current_delay;
    }
    last_time = current_time;

    esp_lcd_panel_draw_bitmap(g_panel_handle, area->x1, area->y1, area->x2, area->y2, px_map);
    lv_disp_flush_ready(disp);
}

static void lv_tick_task(void* arg)
{
    (void)arg;
    lv_tick_inc(LV_TICK_PERIOD_MS);
}

static float theta = 0;
static float dtheta = M_TWOPI/20.f;

static uint32_t last_frame = 0;
static void timer_cb(lv_timer_t* timer)
{
    uint32_t current_frame = esp_timer_get_time();

    float dt = (current_frame - last_frame) / 1000000.0;
    theta += dtheta * dt;

    float x = sin(theta);
    float y = cos(theta);
    lv_obj_set_pos(timer->user_data, (int)(x * 210), (int)(y * 210));
    lv_obj_invalidate(timer->user_data);
    _lv_disp_refr_timer(NULL);

    theta = fmod(theta, M_TWOPI);
    last_frame = current_frame;
}

void app_main(void)
{
    ESP_LOGI(__func__, "Program started");

    ESP_ERROR_CHECK(pca9554_init());
    ESP_LOGI(__func__, "I2C initialized successfully");

    ESP_ERROR_CHECK(pca9554_enable_backlight(true));
    ESP_LOGI(__func__, "Backlight enabled");

    ESP_ERROR_CHECK(lcd_controller_init());

    ESP_ERROR_CHECK(buttons_init());
    ESP_LOGI(__func__, "Buttons initialized");

    register_button_rising_edge_callback(button_up_cb);

    lv_init();
    ESP_LOGI(__func__, "LVGL initialized");

    void* buf1 = NULL;
    void* buf2 = NULL;
    esp_lcd_rgb_panel_get_frame_buffer(g_panel_handle, 2, &buf1, &buf2);

    lv_display_t* display = lv_display_create(LCD_H_RES, LCD_V_RES);
    if (!display)
    {
        ESP_LOGE(__func__, "Failed to create display");
    }
    lv_display_set_default(display);
    lv_display_set_flush_cb(display, lcd_flush_callback);
    lv_timer_del(display->refr_timer);
    display->refr_timer = NULL;

    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);

    lv_display_set_buffers(display, buf1, buf2, LCD_H_RES * LCD_V_RES * sizeof(uint16_t),
                           LV_DISPLAY_RENDER_MODE_FULL);

    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task, .dispatch_method = ESP_TIMER_ISR, .name = "periodic_timer"};
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000));

    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), LV_PART_MAIN);

    static lv_point_precise_t line_points[] = { {240, 240}, {480, 240} };
    static lv_style_t style_line;
    lv_style_init(&style_line);
    lv_style_set_line_width(&style_line, 2);
    lv_style_set_line_color(&style_line, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_line_rounded(&style_line, true);

    lv_obj_t* line = lv_line_create(lv_screen_active());
    // lv_obj_center(line);
    lv_line_set_points(line, line_points, 2);
    lv_obj_add_style(line, &style_line, 0);

    LV_DRAW_BUF_DEFINE(draw_buf, 50, 50, LV_COLOR_FORMAT_ARGB8888);
    lv_obj_t * canvas = lv_canvas_create(lv_screen_active());
    lv_canvas_set_draw_buf(canvas, &draw_buf);
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_TRANSP);
    lv_obj_center(canvas);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_grad.dir = LV_GRAD_DIR_HOR;
    dsc.bg_opa = LV_OPA_COVER;
    dsc.bg_grad.stops[0].color = lv_palette_main(LV_PALETTE_GREEN);
    dsc.bg_grad.stops[0].opa = LV_OPA_100;
    dsc.bg_grad.stops[1].color = lv_palette_darken(LV_PALETTE_GREEN, 1);
    dsc.bg_grad.stops[1].opa = LV_OPA_90;
    dsc.bg_color = lv_palette_main(LV_PALETTE_GREEN);
    dsc.border_width = 1;
    dsc.border_opa = LV_OPA_90;
    dsc.border_color = lv_palette_darken(LV_PALETTE_GREEN, 3);
    dsc.radius = LV_RADIUS_CIRCLE;

    lv_area_t coords = {5, 5, 45, 45};

    lv_draw_rect(&layer, &dsc, &coords);

    lv_canvas_finish_layer(canvas, &layer);

    lv_obj_set_pos(canvas, 240-25, 0);

    lv_timer_create(timer_cb, 15, canvas);
#if MIDI_DEVICE
    ESP_LOGI(TAG, "USB initialization");
    midi_usb_init();
#endif

    uint32_t lasttime = esp_timer_get_time();

    while (true)
    {
        uint32_t delay = lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(delay));
        vTaskDelay(1);

        uint32_t currenttime = esp_timer_get_time();
        uint32_t diff = currenttime - lasttime;
        if (diff > 10000000)
        {
            lasttime = currenttime;
            ESP_LOGI(__func__, "High watermark: %" PRIu32 "ms, Low watermark: %" PRIu32 "ms", highwatermark / 1000,
                     lowwatermark / 1000);
            ESP_LOGI(__func__, "Current delay: %" PRIu32 "ms", current_delay / 1000);

            highwatermark = 0;
            lowwatermark = 0xFFFFFFFF;
        }
    }
}
