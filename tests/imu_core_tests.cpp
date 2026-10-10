#include "gravity_estimator.h"
#include "latest_value.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <thread>

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    std::exit(EXIT_FAILURE); \
} } while (false)

namespace
{
struct Triple
{
    int a = 0;
    int b = 0;
    int c = 0;
};

float angle_between_deg(const imu::Vec3& a, const imu::Vec3& b)
{
    const float dot = a.x * b.x + a.y * b.y + a.z * b.z;
    const float lengths = std::sqrt((a.x * a.x + a.y * a.y + a.z * a.z) * (b.x * b.x + b.y * b.y + b.z * b.z));
    return std::acos(std::fmin(1.0f, dot / lengths)) * 180.0f / 3.14159265f;
}

void gravity_estimator_tests()
{
    constexpr float dt = 0.01f;
    constexpr float g = 9.80665f;
    {
        // Spinning in the screen plane at one turn per second: the gyro keeps the estimate on target.
        constexpr float omega = 2.0f * 3.14159265f;
        filter::GravityEstimator estimator(0.5f);
        float worst_estimator = 0.0f;
        for (int i = 0; i <= 100; ++i)
        {
            const float angle = -omega * dt * static_cast<float>(i);
            const imu::Vec3 truth{-g * std::sin(angle), g * std::cos(angle), 0.0f};
            const imu::Vec3 gyro{0.0f, 0.0f, omega};
            const float step = i == 0 ? 0.0f : dt;
            estimator.update(truth, gyro, step);
            if (i >= 25)
            {
                worst_estimator = std::fmax(worst_estimator, angle_between_deg(estimator.value(), truth));
            }
        }
        CHECK(worst_estimator < 3.0f);
    }
    {
        // A gyro bias only tilts the estimate by bias * time constant.
        filter::GravityEstimator estimator(0.5f);
        const imu::Vec3 truth{0.0f, g, 0.0f};
        for (int i = 0; i <= 500; ++i)
        {
            estimator.update(truth, {0.0f, 0.0f, 0.05f}, i == 0 ? 0.0f : dt);
        }
        CHECK(angle_between_deg(estimator.value(), truth) < 3.0f);
    }
    {
        // Jitter on the accelerometer is smoothed.
        filter::GravityEstimator estimator(0.5f);
        const imu::Vec3 truth{0.0f, g, 0.0f};
        estimator.update(truth, {}, 0.0f);
        float worst = 0.0f;
        for (int i = 1; i <= 200; ++i)
        {
            const float jitter = (i % 2 == 0) ? 2.0f : -2.0f;
            estimator.update({jitter, g, 0.0f}, {}, dt);
            worst = std::fmax(worst, angle_between_deg(estimator.value(), truth));
        }
        CHECK(worst < 1.0f);
    }
}

void latest_value_tests()
{
    {
        LatestValue<int> value;
        CHECK(!value.load().has_value());
        value.publish(3);
        value.publish(7);
        CHECK(value.load() == 7);
    }
    {
        LatestValue<Triple> value;
        std::atomic<bool> done = false;
        std::thread writer([&] {
            for (int i = 1; i <= 100'000; ++i)
            {
                value.publish({i, i, i});
            }
            done = true;
        });
        int last = 0;
        while (!done)
        {
            const auto triple = value.load();
            if (triple)
            {
                CHECK(triple->a == triple->b && triple->b == triple->c);
                CHECK(triple->a >= last);
                last = triple->a;
            }
        }
        writer.join();
        CHECK(value.load()->a == 100'000);
    }
}
} // namespace

int main()
{
    gravity_estimator_tests();
    latest_value_tests();
    return EXIT_SUCCESS;
}
