#include "gui/imu_readout.h"

#include <cstdio>

#include <esp_check.h>
#include <lvgl.h>

namespace gui
{
esp_err_t ImuReadout::create()
{
    ESP_RETURN_ON_FALSE(label_ == nullptr, ESP_ERR_INVALID_STATE, "imu_readout", "IMU readout already created");
    label_ = lv_label_create(lv_screen_active());
    ESP_RETURN_ON_FALSE(label_ != nullptr, ESP_ERR_NO_MEM, "imu_readout", "Failed to create IMU readout");
    lv_obj_set_style_text_color(label_, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_align(label_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_text(label_, "IMU: no data");
    lv_obj_align(label_, LV_ALIGN_CENTER, 0, 0);
    return ESP_OK;
}

void ImuReadout::show(const imu::Vec3& up, const imu::Vec3& gyro, const imu::Vec3& magnetic)
{
    if (label_ == nullptr)
    {
        return;
    }
    char text[128];
    std::snprintf(text, sizeof(text), "a %+6.2f %+6.2f %+6.2f\ng %+6.2f %+6.2f %+6.2f\nm %+6.1f %+6.1f %+6.1f",
                  up.x, up.y, up.z, gyro.x, gyro.y, gyro.z, magnetic.x, magnetic.y, magnetic.z);
    lv_label_set_text(label_, text);
}
} // namespace gui
