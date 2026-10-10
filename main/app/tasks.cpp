#include "tasks.h"

#include <chrono>
#include <cstdint>

#include <esp_check.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "gui/port.h"
#include "lcd_controller.h"
#include "simulation.h"

#if CONFIG_QUALIA_MODE_MIDI
#include "bounce_midi.h"
#include "midi_handoff.h"
#endif

namespace
{
constexpr const char* TAG = "tasks";

struct TaskConfig
{
    const char* name;
    std::uint32_t stack_bytes;
    UBaseType_t priority;
    BaseType_t core;
};

// Every application task. TinyUSB and esp_timer create their own tasks outside this list.
constexpr TaskConfig SENSORS_TASK{"sensors_task", 4096, 3, 1};
constexpr TaskConfig UI_TASK{"ui_task", 16384, 1, 0};
#if CONFIG_QUALIA_MODE_MIDI
constexpr TaskConfig MIDI_TASK{"midi_task", 2048, 5, tskNO_AFFINITY};
#endif

constexpr auto SENSORS_PERIOD = app::BoardInputs::PERIOD;
static_assert(app::BoardImu::PERIOD % SENSORS_PERIOD == SENSORS_PERIOD.zero(),
              "The IMU period must be a multiple of the sensors tick");
constexpr std::uint32_t IMU_DIVIDER = app::BoardImu::PERIOD / SENSORS_PERIOD;
static_assert(pdMS_TO_TICKS(SENSORS_PERIOD.count()) > 0, "The sensors tick is shorter than one FreeRTOS tick");

app::BoardImu* g_sensors_imu = nullptr;
app::BoardInputs* g_sensors_inputs = nullptr;
const app::BoardImu* g_ui_imu = nullptr;
BounceCallback g_on_bounce = nullptr;
#if CONFIG_QUALIA_MODE_MIDI
modules::Midi* g_midi = nullptr;
app::MidiHandoff g_midi_handoff;
#endif

std::chrono::microseconds now()
{
    return std::chrono::microseconds(esp_timer_get_time());
}

void sensors_task(void*)
{
    ESP_LOGI(TAG, "Sensors task started (imu %s, inputs %s)", g_sensors_imu != nullptr ? "on" : "off",
             g_sensors_inputs != nullptr ? "on" : "off");
    TickType_t last_wake_time = xTaskGetTickCount();
    for (std::uint32_t tick = 0;; ++tick)
    {
        const BaseType_t on_time = xTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(SENSORS_PERIOD.count()));
        if (on_time == pdFALSE)
        {
            // Overran (e.g. an I2C timeout): run once now, then restart the schedule instead of replaying every
            // missed period back to back, which would also collapse the debounce window.
            last_wake_time = xTaskGetTickCount();
        }
        if (g_sensors_inputs != nullptr)
        {
            g_sensors_inputs->step(now());
        }
        if (g_sensors_imu != nullptr && tick % IMU_DIVIDER == 0)
        {
            g_sensors_imu->step(now());
        }
#if CONFIG_QUALIA_IMU_DEBUG
        if (tick % 2000 == 0)
        {
            ESP_LOGI(TAG, "Sensors task stack high-water mark: %u bytes",
                     static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
        }
#endif
    }
}

void ui_task(void*)
{
    ESP_LOGI(TAG, "UI task started");

    ESP_ERROR_CHECK(gui::init());
    ESP_ERROR_CHECK(init_simulation(*g_ui_imu, g_on_bounce));

    for (;;)
    {
        gui::run_timers();
        update_simulation();
        vTaskDelay(1);
    }
}

#if CONFIG_QUALIA_MODE_MIDI
void midi_task(void*)
{
    std::optional<std::chrono::microseconds> bootloader_deadline;
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(modules::Midi::PERIOD.count()));
        g_midi->step(now());
        ESP_ERROR_CHECK(g_midi_handoff.set_online(g_midi->ready_for_notes()));
        if (g_midi->bootloader_requested())
        {
            if (!bootloader_deadline)
            {
                bootloader_deadline = now() + std::chrono::milliseconds(250);
            }
            if (g_midi->bootloader_ready() || now() >= *bootloader_deadline)
            {
                if (!g_midi->bootloader_ready())
                {
                    ESP_LOGW(TAG, "Bootloader entry timed out waiting for MIDI note cleanup");
                }
                ESP_ERROR_CHECK(drivers::MidiUsb::enter_bootloader());
            }
            continue;
        }
        if (g_midi->can_submit())
        {
            const auto note = g_midi_handoff.receive();
            if (note)
            {
                ESP_ERROR_CHECK(g_midi->submit(*note));
                g_midi->step(now());
            }
        }
    }
}
#endif

esp_err_t start(const TaskConfig& config, TaskFunction_t entry)
{
    const BaseType_t created =
        xTaskCreatePinnedToCore(entry, config.name, config.stack_bytes, nullptr, config.priority, nullptr, config.core);
    ESP_RETURN_ON_FALSE(created == pdPASS, ESP_ERR_NO_MEM, TAG, "Failed to create %s", config.name);
    return ESP_OK;
}
} // namespace

namespace app
{
esp_err_t start_sensors_task(app::BoardImu& imu, app::BoardInputs& inputs)
{
    g_sensors_imu = imu.is_initialized() ? &imu : nullptr;
    g_sensors_inputs = inputs.is_initialized() ? &inputs : nullptr;
    return start(SENSORS_TASK, sensors_task);
}

esp_err_t start_ui_task(const app::BoardImu& imu, BounceCallback on_bounce)
{
    ESP_RETURN_ON_FALSE(drivers::Lcd::is_initialized(), ESP_ERR_INVALID_STATE, TAG, "LCD is not initialized");
    g_ui_imu = &imu;
    g_on_bounce = on_bounce;
    return start(UI_TASK, ui_task);
}

#if CONFIG_QUALIA_MODE_MIDI
esp_err_t start_midi_task(modules::Midi& midi)
{
    ESP_RETURN_ON_FALSE(midi.is_initialized(), ESP_ERR_INVALID_STATE, TAG, "MIDI is not initialized");
    ESP_RETURN_ON_ERROR(g_midi_handoff.init(), TAG, "Failed to initialize MIDI handoff");
    g_midi = &midi;
    return start(MIDI_TASK, midi_task);
}

void publish_bounce_midi(const physics::Bounce& bounce)
{
    const auto note = bounce_note(bounce);
    if (!note)
    {
        ESP_LOGE(TAG, "Cannot map invalid bounce to MIDI");
        return;
    }
    static esp_err_t last_error = ESP_OK;
    const esp_err_t err = g_midi_handoff.publish(*note);
    if (err != ESP_OK && err != last_error)
    {
        ESP_LOGW(TAG, "Discarding MIDI bounce: %s", err == ESP_ERR_NO_MEM ? "queue full" : "USB MIDI offline");
    }
    last_error = err;
}
#endif
} // namespace app
