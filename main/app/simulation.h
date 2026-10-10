#pragma once

#include <esp_err.h>

#include "board.h"

/**
 * @brief imu must outlive the simulation.
 */
esp_err_t init_simulation(const app::BoardImu& imu);
void update_simulation();