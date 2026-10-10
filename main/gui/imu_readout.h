#pragma once

#include <esp_err.h>

#include "sensor_types.h"

typedef struct _lv_obj_t lv_obj_t;

namespace gui
{
/**
 * @brief Centered three-line text readout of the IMU state.
 */
class ImuReadout
{
  public:
    ImuReadout() = default;
    ImuReadout(const ImuReadout&) = delete;
    ImuReadout& operator=(const ImuReadout&) = delete;

    esp_err_t create();
    void show(const imu::Vec3& up, const imu::Vec3& gyro, const imu::Vec3& magnetic);

  private:
    lv_obj_t* label_ = nullptr;
};
} // namespace gui
