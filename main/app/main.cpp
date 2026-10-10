#include <array>
#include <type_traits>

#include <esp_log.h>

#include "board.h"
#include "hardware_driver.h"
#include "i2c_bus.h"
#include "lcd_controller.h"
#include "modules/module.h"
#include "pca9554_controller.h"
#include "tasks.h"

#if CONFIG_QUALIA_MODE_MIDI
#include "modules/midi.h"
#endif

namespace
{
constexpr const char* TAG = "main";

enum class Policy
{
    Required, // abort startup on failure
    Optional, // log a warning and continue
};

struct DriverInit
{
    const char* name;
    esp_err_t (*init)();
    Policy policy;
};

template <drivers::HardwareDriver T>
constexpr DriverInit driver(const char* name, Policy policy)
{
    return {name, &T::init, policy};
}

// Shared drivers that no single module owns. Initialized in order; every driver must come after the drivers it
// depends on.
constexpr std::array DRIVERS{
    driver<drivers::I2cBus>("i2c_bus", Policy::Required),
    driver<drivers::Pca9554>("pca9554", Policy::Required),
    driver<drivers::Lcd>("lcd", Policy::Required),
};

void on_input(modules::Input which)
{
    const bool up = which == modules::Input::Up;
    ESP_LOGI(TAG, "Button %s pressed", up ? "up" : "down");
    const esp_err_t err = drivers::Lcd::set_backlight(up);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Backlight %s failed: %s", up ? "on" : "off", esp_err_to_name(err));
    }
}

app::BoardImu g_imu;
app::BoardInputs g_inputs(on_input);
#if CONFIG_QUALIA_MODE_MIDI
modules::Midi g_midi;
#endif

struct ModuleInit
{
    const char* name;
    esp_err_t (*init)();
    Policy policy;
};

template <auto& M>
    requires modules::Module<std::remove_reference_t<decltype(M)>>
constexpr ModuleInit module(const char* name, Policy policy)
{
    return {name, [] { return M.init(); }, policy};
}

// Initialized in order after DRIVERS. The tasks started in app_main then step every initialized module.
constexpr std::array MODULES{
    module<g_imu>("imu", Policy::Optional),
    module<g_inputs>("inputs", Policy::Required),
#if CONFIG_QUALIA_MODE_MIDI
    module<g_midi>("midi", Policy::Required),
#endif
};

// Aborts on a required failure; otherwise returns whether the step succeeded.
bool check(const char* name, const char* step, Policy policy, esp_err_t err)
{
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "%s: %s done", name, step);
        return true;
    }
    if (policy == Policy::Optional)
    {
        ESP_LOGW(TAG, "%s: %s failed (%s); continuing without it", name, step, esp_err_to_name(err));
        return false;
    }
    ESP_LOGE(TAG, "%s: %s failed", name, step);
    ESP_ERROR_CHECK(err);
    return false;
}
} // namespace

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Program started");

    for (const DriverInit& entry : DRIVERS)
    {
        check(entry.name, "init", entry.policy, entry.init());
    }

    for (const ModuleInit& entry : MODULES)
    {
        check(entry.name, "init", entry.policy, entry.init());
    }

    check("sensors_task", "start", Policy::Required, app::start_sensors_task(g_imu, g_inputs));
    check("ui_task", "start", Policy::Required, app::start_ui_task(g_imu));
#if CONFIG_QUALIA_MODE_MIDI
    check("midi_task", "start", Policy::Required, app::start_midi_task(g_midi));
#endif
}
