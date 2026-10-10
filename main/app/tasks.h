#pragma once

#include <esp_err.h>
#include <sdkconfig.h>

#include "board.h"

#if CONFIG_QUALIA_MODE_MIDI
#include "modules/midi.h"
#endif

namespace app
{
/**
 * @brief Starts the sensors task on core 1. It steps inputs every Inputs::PERIOD and imu every Imu::PERIOD,
 * skipping a module whose init() failed.
 *
 * Both modules must outlive the task.
 */
esp_err_t start_sensors_task(app::BoardImu& imu, app::BoardInputs& inputs);

/**
 * @brief Starts the UI task on core 0: sets up LVGL and the simulation, then renders and steps them forever.
 *
 * Requires drivers::Lcd. imu must outlive the task.
 */
esp_err_t start_ui_task(const app::BoardImu& imu);

#if CONFIG_QUALIA_MODE_MIDI
/**
 * @brief Starts the MIDI task, which steps midi every Midi::PERIOD. Requires an initialized midi that outlives the
 * task.
 */
esp_err_t start_midi_task(modules::Midi& midi);
#endif
} // namespace app
