#pragma once

#include <esp_err.h>

#include "display_helpers.h"

typedef struct _lv_obj_t lv_obj_t;
typedef struct _lv_draw_buf_t lv_draw_buf_t;

namespace gui
{
/**
 * @brief A shaded green disc on the active screen, positioned by its center in panel pixels.
 */
class BallView
{
  public:
    BallView() = default;
    BallView(const BallView&) = delete;
    BallView& operator=(const BallView&) = delete;

    /**
     * @param diameter Disc diameter in pixels; at most 50.
     */
    esp_err_t create(int diameter);
    void move_to(display::Point center);
    void destroy();

  private:
    lv_obj_t* canvas_ = nullptr;
    lv_draw_buf_t* draw_buf_ = nullptr;
};
} // namespace gui
