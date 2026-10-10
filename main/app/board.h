#pragma once

#include <cstdint>
#include <expected>

#include <esp_err.h>

#include "lis3mdl.h"
#include "lsm6dsox.h"
#include "modules/imu.h"
#include "modules/inputs.h"
#include "pca9554_controller.h"

namespace app
{
/**
 * @brief The IMU on this board: an Adafruit 4517 breakout (LSM6DSOX + LIS3MDL) on the STEMMA QT port.
 */
using BoardImu = modules::Imu<drivers::Lsm6dsox, drivers::Lis3mdl>;

/**
 * @brief The Qualia's up/down buttons, wired to the PCA9554 expander.
 */
struct BoardButtons
{
    static bool is_initialized()
    {
        return drivers::Pca9554::is_initialized();
    }

    static std::expected<bool, esp_err_t> level(modules::Input which)
    {
        return drivers::Pca9554::get_level(which == modules::Input::Up ? drivers::Pca9554::BUTTON_UP
                                                                       : drivers::Pca9554::BUTTON_DOWN);
    }
};

using BoardInputs = modules::Inputs<BoardButtons>;
} // namespace app
