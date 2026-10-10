#include "gui/imu_readout.h"

#include <array>
#include <cstdio>

#include <esp_check.h>
#include <lvgl.h>

namespace gui
{
ImuReadout::~ImuReadout()
{
    destroy();
}

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

esp_err_t ImuReadout::show(const imu::Vec3& up, const imu::Vec3& gyro, const imu::Vec3& magnetic)
{
    ESP_RETURN_ON_FALSE(label_ != nullptr, ESP_ERR_INVALID_STATE, "imu_readout", "IMU readout is not created");
    std::array<char, 128> text{};
    const int length = std::snprintf(text.data(), text.size(),
                                     "a %+6.2f %+6.2f %+6.2f\ng %+6.2f %+6.2f %+6.2f\nm %+6.1f %+6.1f %+6.1f", up.x,
                                     up.y, up.z, gyro.x, gyro.y, gyro.z, magnetic.x, magnetic.y, magnetic.z);
    ESP_RETURN_ON_FALSE(length >= 0 && static_cast<std::size_t>(length) < text.size(), ESP_ERR_INVALID_SIZE,
                        "imu_readout", "IMU text does not fit the readout");
    lv_label_set_text(label_, text.data());
    return ESP_OK;
}

void ImuReadout::destroy()
{
    if (label_ != nullptr)
    {
        lv_obj_delete(label_);
        label_ = nullptr;
    }
}
} // namespace gui
