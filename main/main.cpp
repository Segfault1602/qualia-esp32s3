#include <stdio.h>

#include <freertos/FreeRTOS.h>

#include <esp_app_trace.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/task.h>
#include <lvgl.h>
#include <math.h>
#include <src/display/lv_display_private.h>

#include "buttons.h"
#include "defines.h"
#include "init_operations.h"
#include "lcd_controller.h"
#include "pca9554_controller.h"
#include "renderer.h"

#define MIDI_DEVICE 0
#if MIDI_DEVICE
#include "midi_usb.h"
#endif // MIDI_DEVICE

static uint32_t highwatermark = 0;
static uint32_t lowwatermark = 0xFFFFFFFF;
static uint32_t current_delay = 0;
static uint32_t start = 0;

void start_profiler()
{
    start = esp_timer_get_time();
}

void stop_profiler()
{
    uint32_t end = esp_timer_get_time();
    current_delay = end - start;
    if (current_delay > highwatermark)
    {
        highwatermark = current_delay;
    }
    if (current_delay < lowwatermark)
    {
        lowwatermark = current_delay;
    }
}

void print_profiler()
{
    ESP_LOGI(__func__, "High watermark: %" PRIu32 ", Low watermark: %" PRIu32 "", highwatermark, lowwatermark);
    ESP_LOGI(__func__, "Current delay: %" PRIu32 "", current_delay);

    highwatermark = 0;
    lowwatermark = 0xFFFFFFFF;
}

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

extern "C" void app_main(void)
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

    esp_apptrace_init();

    constexpr char trace_buf[] = "Trace Start\n";
    esp_apptrace_write(ESP_APPTRACE_DEST_TRAX, trace_buf, strlen(trace_buf), 100);

    BaseType_t result = xTaskCreatePinnedToCore(render_task, "render_task", 16384, NULL, 0, NULL, 0);
    if (result != pdPASS)
    {
        ESP_LOGE(__func__, "Failed to create render task");
    }

#if MIDI_DEVICE
    ESP_LOGI(TAG, "USB initialization");
    midi_usb_init();
#endif
}
