#pragma once

#include <esp_err.h>

#include "board.h"

/**
 * @brief Shows the latest IMU state as text, refreshed at 4 Hz. Call both functions from the UI task; imu must
 * outlive the overlay.
 */
esp_err_t imu_overlay_create(const app::BoardImu& imu);
void imu_overlay_update();
