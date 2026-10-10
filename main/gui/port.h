#pragma once

#include <esp_err.h>

namespace gui
{
/**
 * @brief Sets up LVGL on drivers::Lcd: double-buffered direct rendering, presented on VSYNC, with a 1 ms tick.
 *
 * Requires drivers::Lcd. Call it from the task that runs LVGL: that task is the one woken on VSYNC, and every
 * other LVGL call must come from it too.
 */
esp_err_t init();

/**
 * @brief Runs due LVGL timers: renders and presents a frame when anything changed. Call from the init() task.
 */
void run_timers();
} // namespace gui
