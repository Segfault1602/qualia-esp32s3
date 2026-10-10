#pragma once

#include <esp_err.h>

#include "board.h"
#include "physics_world.h"

using BounceCallback = void (*)(const physics::Bounce&);
/**
 * @brief imu must outlive the simulation.
 */
esp_err_t init_simulation(const app::BoardImu& imu, BounceCallback on_bounce = nullptr);
void update_simulation();