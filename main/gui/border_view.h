#pragma once

#include <esp_err.h>

#include "display_helpers.h"

typedef struct _lv_obj_t lv_obj_t;

namespace gui
{
/**
 * @brief Owns a blue ring whose inner edge is a circle in panel pixels.
 *
 * Create and destroy on the UI task while the active screen is valid.
 */
class BorderView
{
  public:
    BorderView() = default;
    ~BorderView();
    BorderView(const BorderView&) = delete;
    BorderView& operator=(const BorderView&) = delete;

    esp_err_t create(display::Point center, int inner_radius);
    void destroy();

  private:
    lv_obj_t* ring_ = nullptr;
};
} // namespace gui
