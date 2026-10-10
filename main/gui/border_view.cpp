#include "gui/border_view.h"

#include <limits>

#include <esp_check.h>
#include <lvgl.h>

namespace gui
{
BorderView::~BorderView()
{
    destroy();
}

esp_err_t BorderView::create(display::Point center, int inner_radius)
{
    constexpr int RING_WIDTH = 2;
    ESP_RETURN_ON_FALSE(ring_ == nullptr, ESP_ERR_INVALID_STATE, "border_view", "Border view already created");
    ESP_RETURN_ON_FALSE(inner_radius > 0 && inner_radius <= std::numeric_limits<int>::max() / 2 - RING_WIDTH,
                        ESP_ERR_INVALID_ARG, "border_view", "Invalid border radius");
    const int diameter = 2 * (inner_radius + RING_WIDTH);
    const auto origin = display::centered_position(center, diameter, diameter);

    ring_ = lv_obj_create(lv_screen_active());
    ESP_RETURN_ON_FALSE(ring_ != nullptr, ESP_ERR_NO_MEM, "border_view", "Failed to create border ring");
    lv_obj_remove_style_all(ring_);
    lv_obj_set_pos(ring_, origin.x, origin.y);
    lv_obj_set_size(ring_, diameter, diameter);
    lv_obj_set_style_radius(ring_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(ring_, RING_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_border_color(ring_, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
    lv_obj_set_style_border_opa(ring_, LV_OPA_COVER, LV_PART_MAIN);
    return ESP_OK;
}

void BorderView::destroy()
{
    if (ring_ != nullptr)
    {
        lv_obj_delete(ring_);
        ring_ = nullptr;
    }
}
} // namespace gui
