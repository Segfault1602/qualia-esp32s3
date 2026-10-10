#pragma once

#include <esp_err.h>

#include "display_helpers.h"

namespace gui
{
/**
 * @brief Draws a blue ring on the active screen whose inner edge is a circle of inner_radius pixels around center.
 */
esp_err_t create_border_ring(display::Point center, int inner_radius);
} // namespace gui
