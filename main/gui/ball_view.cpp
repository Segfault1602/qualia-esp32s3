#include "gui/ball_view.h"

#include <esp_check.h>
#include <esp_log.h>
#include <lvgl.h>

namespace
{
constexpr const char* TAG = "ball_view";
constexpr int DRAW_BUF_WIDTH = 50;
constexpr int DRAW_BUF_HEIGHT = 50;
} // namespace

namespace gui
{
BallView::~BallView()
{
    destroy();
}

esp_err_t BallView::create(int diameter)
{
    ESP_RETURN_ON_FALSE(canvas_ == nullptr, ESP_ERR_INVALID_STATE, TAG, "Ball view already created");
    ESP_RETURN_ON_FALSE(diameter > 0 && diameter <= DRAW_BUF_WIDTH, ESP_ERR_INVALID_ARG, TAG,
                        "Ball diameter %d does not fit the draw buffer", diameter);

    draw_buf_ = lv_draw_buf_create(DRAW_BUF_WIDTH, DRAW_BUF_HEIGHT, LV_COLOR_FORMAT_ARGB8888, 0);
    ESP_RETURN_ON_FALSE(draw_buf_ != nullptr, ESP_ERR_NO_MEM, TAG, "Failed to allocate ball draw buffer");
    canvas_ = lv_canvas_create(lv_screen_active());
    if (canvas_ == nullptr)
    {
        destroy();
        ESP_LOGE(TAG, "Failed to create ball canvas");
        return ESP_ERR_NO_MEM;
    }
    lv_canvas_set_draw_buf(canvas_, draw_buf_);
    lv_canvas_fill_bg(canvas_, lv_color_black(), LV_OPA_TRANSP);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas_, &layer);

    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_opa = LV_OPA_COVER;
    dsc.bg_color = lv_palette_main(LV_PALETTE_GREEN);
    dsc.border_width = 5;
    dsc.border_opa = LV_OPA_90;
    dsc.border_color = lv_palette_darken(LV_PALETTE_GREEN, 3);
    dsc.radius = LV_RADIUS_CIRCLE;

    const int first = DRAW_BUF_WIDTH / 2 - diameter / 2;
    lv_area_t coords = {first, first, first + diameter - 1, first + diameter - 1};
    lv_draw_rect(&layer, &dsc, &coords);

    lv_canvas_finish_layer(canvas_, &layer);
    return ESP_OK;
}

esp_err_t BallView::move_to(display::Point center)
{
    ESP_RETURN_ON_FALSE(canvas_ != nullptr, ESP_ERR_INVALID_STATE, TAG, "Ball view is not created");
    const auto origin = display::centered_position(center, DRAW_BUF_WIDTH, DRAW_BUF_HEIGHT);
    lv_obj_set_pos(canvas_, origin.x, origin.y);
    return ESP_OK;
}

void BallView::destroy()
{
    if (canvas_ != nullptr)
    {
        lv_obj_delete(canvas_);
        canvas_ = nullptr;
    }
    if (draw_buf_ != nullptr)
    {
        lv_draw_buf_destroy(draw_buf_);
        draw_buf_ = nullptr;
    }
}
} // namespace gui
