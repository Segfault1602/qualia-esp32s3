#pragma once

namespace imu
{
struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct MotionSample
{
    Vec3 accel; // m/s^2, specific force: reads +1 g pointing up at rest
    Vec3 gyro;  // rad/s
};

enum class Error
{
    Bus,
    WrongDevice,
    Timeout,
};

constexpr const char* to_string(Error error)
{
    switch (error)
    {
    case Error::Bus:
        return "bus error";
    case Error::WrongDevice:
        return "unexpected WHO_AM_I";
    case Error::Timeout:
        return "timeout";
    }
    return "unknown";
}
} // namespace imu
