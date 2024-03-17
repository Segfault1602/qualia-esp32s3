#include "buttons.h"

#include <esp_check.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "pca9554_controller.h"
#include <defines.h>

#define BUTTONS_TAG "BUTTONS"

#define BUTTON_TASK_FREQUENCY_MS (1)

static button_rising_edge_cb_t g_button_rising_edge_cb = NULL;
static uint8_t g_button_state[2] = {0, 0};

static void buttons_task(void* pvParameters);

static uint8_t map_button_to_pca9554_pin(uint8_t button)
{
    switch (button)
    {
    case BUTTON_UP:
        return PCA9554_BUTTON_UP;
    case BUTTON_DOWN:
        return PCA9554_BUTTON_DOWN;
    default:
        return 0;
    }
}

esp_err_t buttons_init()
{
    ESP_RETURN_ON_FALSE(pca9554_is_init(), ESP_ERR_INVALID_STATE, BUTTONS_TAG, "PCA9554 is not initialized");

    ESP_RETURN_ON_ERROR(pca9554_set_pin_mode(PCA9554_BUTTON_UP, PCA9554_PIN_MODE_INPUT), BUTTONS_TAG,
                        "Failed to set pin mode for button up");
    ESP_RETURN_ON_ERROR(pca9554_set_pin_mode(PCA9554_BUTTON_DOWN, PCA9554_PIN_MODE_INPUT), BUTTONS_TAG,
                        "Failed to set pin mode for button down");

    BaseType_t ret = xTaskCreate(buttons_task, "buttons_task", 4024, NULL, BUTTON_TASK_PRIORITY, NULL);
    if (ret != pdPASS)
    {
        ESP_LOGE(BUTTONS_TAG, "Failed to create buttons task with error %d", ret);
        return ESP_FAIL;
    }

    return ESP_OK;
}

void register_button_rising_edge_callback(button_rising_edge_cb_t cb)
{
    g_button_rising_edge_cb = cb;
}

static void buttons_task(void* pvParameters)
{
    ESP_LOGI(BUTTONS_TAG, "Buttons task started");
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t frequency = pdMS_TO_TICKS(BUTTON_TASK_FREQUENCY_MS);

    for (;;)
    {
        xTaskDelayUntil(&last_wake_time, frequency);
        TickType_t now = xTaskGetTickCount();
        last_wake_time = now;

        for (uint8_t i = 0; i < BUTTON_COUNT; i++)
        {
            uint8_t state = 0;
            pca9554_get_pin_state(map_button_to_pca9554_pin(i), &state);
            g_button_state[i] = (g_button_state[i] << 1) | state;

            if (g_button_state[i] == 0x80)
            {
                if (g_button_rising_edge_cb != NULL)
                {
                    g_button_rising_edge_cb(i);
                }
            }
        }
    }
}