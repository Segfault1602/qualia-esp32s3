#pragma once

#include <esp_err.h>

#include "sensor_types.h"

typedef struct _lv_obj_t lv_obj_t;

namespace gui
{
/**
 * @brief Owns a centered three-line text readout of the IMU state.
 *
 * Create, show, and destroy on the UI task while the active screen is valid.
 */
class ImuReadout
{
  public:
    ImuReadout() = default;
    ~ImuReadout();
    ImuReadout(const ImuReadout&) = delete;
    ImuReadout& operator=(const ImuReadout&) = delete;

    esp_err_t create();
    esp_err_t show(const imu::Vec3& up, const imu::Vec3& gyro, const imu::Vec3& magnetic);
    void destroy();

  private:
    lv_obj_t* label_ = nullptr;
};
} // namespace gui
