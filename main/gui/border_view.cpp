#include "gui/border_view.h"

#include <esp_check.h>
#include <lvgl.h>

namespace gui
{
esp_err_t create_border_ring(display::Point center, int inner_radius)
{
    constexpr int RING_WIDTH = 2;
    const int diameter = 2 * (inner_radius + RING_WIDTH);
    const auto origin = display::centered_position(center, diameter, diameter);

    lv_obj_t* ring = lv_obj_create(lv_screen_active());
    ESP_RETURN_ON_FALSE(ring != nullptr, ESP_ERR_NO_MEM, "border_view", "Failed to create border ring");
    lv_obj_remove_style_all(ring);
    lv_obj_set_pos(ring, origin.x, origin.y);
    lv_obj_set_size(ring, diameter, diameter);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(ring, RING_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_border_color(ring, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
    lv_obj_set_style_border_opa(ring, LV_OPA_COVER, LV_PART_MAIN);
    return ESP_OK;
}
} // namespace gui
