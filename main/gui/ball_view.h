#pragma once

#include <esp_err.h>

#include "display_helpers.h"

typedef struct _lv_obj_t lv_obj_t;
typedef struct _lv_draw_buf_t lv_draw_buf_t;

namespace gui
{
/**
 * @brief Owns a shaded green disc positioned by its center in panel pixels.
 *
 * Create, move, and destroy on the UI task while the active screen is valid.
 */
class BallView
{
  public:
    BallView() = default;
    ~BallView();
    BallView(const BallView&) = delete;
    BallView& operator=(const BallView&) = delete;

    /**
     * @param diameter Disc diameter in pixels; at most 50.
     */
    esp_err_t create(int diameter);
    esp_err_t move_to(display::Point center);
    void destroy();

  private:
    lv_obj_t* canvas_ = nullptr;
    lv_draw_buf_t* draw_buf_ = nullptr;
};
} // namespace gui
