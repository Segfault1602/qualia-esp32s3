#include "modules/imu.h"
#include "modules/module.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <expected>

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    std::exit(EXIT_FAILURE); \
} } while (false)

namespace
{
using namespace std::chrono_literals;

struct FakeMotion
{
    static inline esp_err_t init_result = ESP_OK;
    static inline bool initialized = false;
    static inline std::expected<imu::MotionSample, esp_err_t> next = imu::MotionSample{};
    static inline int reads = 0;

    static void reset()
    {
        init_result = ESP_OK;
        initialized = false;
        next = imu::MotionSample{};
        reads = 0;
    }
    static esp_err_t init()
    {
        initialized = init_result == ESP_OK;
        return init_result;
    }
    static bool is_initialized()
    {
        return initialized;
    }
    static std::expected<imu::MotionSample, esp_err_t> read()
    {
        ++reads;
        return next;
    }
};

struct FakeMagnetic
{
    static inline esp_err_t init_result = ESP_OK;
    static inline bool initialized = false;
    static inline std::expected<imu::Vec3, esp_err_t> next = imu::Vec3{};
    static inline int reads = 0;

    static void reset()
    {
        init_result = ESP_OK;
        initialized = false;
        next = imu::Vec3{};
        reads = 0;
    }
    static esp_err_t init()
    {
        initialized = init_result == ESP_OK;
        return init_result;
    }
    static bool is_initialized()
    {
        return initialized;
    }
    static std::expected<imu::Vec3, esp_err_t> read()
    {
        ++reads;
        return next;
    }
};

using TestImu = modules::Imu<FakeMotion, FakeMagnetic>;
static_assert(modules::Module<TestImu>);

constexpr float G = 9.80665f;

void reset()
{
    FakeMotion::reset();
    FakeMagnetic::reset();
}

bool near(float a, float b)
{
    return std::fabs(a - b) < 1e-5f;
}

void init_tests()
{
    {
        reset();
        FakeMotion::init_result = ESP_ERR_NOT_FOUND;
        TestImu imu;
        CHECK(imu.init() == ESP_ERR_NOT_FOUND);
        CHECK(!imu.is_initialized());
    }
    {
        reset();
        FakeMagnetic::init_result = ESP_ERR_NOT_FOUND;
        TestImu imu;
        CHECK(imu.init() == ESP_OK);
        CHECK(imu.is_initialized());
        imu.step(0us);
        const auto state = imu.latest();
        CHECK(state.has_value());
        CHECK(!state->magnetic.has_value());
        CHECK(FakeMagnetic::reads == 0);
    }
}

void publish_tests()
{
    reset();
    TestImu imu;
    CHECK(imu.init() == ESP_OK);
    CHECK(!imu.latest().has_value());

    FakeMotion::next = imu::MotionSample{{0.0f, G, 0.0f}, {0.1f, 0.2f, 0.3f}};
    FakeMagnetic::next = imu::Vec3{10.0f, 20.0f, 30.0f};
    imu.step(1000us);
    const auto state = imu.latest();
    CHECK(state.has_value());
    // The first sample primes the gravity estimate with the accelerometer reading.
    CHECK(near(state->up.y, G));
    CHECK(near(state->motion.gyro.z, 0.3f));
    CHECK(state->magnetic.has_value());
    CHECK(near(state->magnetic->z, 30.0f));
}

void read_failure_tests()
{
    reset();
    TestImu imu;
    CHECK(imu.init() == ESP_OK);

    FakeMotion::next = std::unexpected(ESP_ERR_TIMEOUT);
    imu.step(0us);
    CHECK(!imu.latest().has_value());
    // A failed motion read skips the whole step, including the magnetometer.
    CHECK(FakeMagnetic::reads == 0);

    FakeMotion::next = imu::MotionSample{{1.0f, 2.0f, 3.0f}, {}};
    imu.step(10ms);
    CHECK(imu.latest().has_value());
    CHECK(near(imu.latest()->motion.accel.x, 1.0f));

    FakeMotion::next = std::unexpected(ESP_ERR_TIMEOUT);
    imu.step(20ms);
    // The previous reading stays published.
    CHECK(near(imu.latest()->motion.accel.x, 1.0f));
}

void magnetic_divider_tests()
{
    reset();
    TestImu imu;
    CHECK(imu.init() == ESP_OK);
    for (int i = 0; i < 21; ++i)
    {
        imu.step(std::chrono::milliseconds(10 * i));
    }
    CHECK(FakeMotion::reads == 21);
    // Steps 0, 10, and 20.
    CHECK(FakeMagnetic::reads == 3);
}

void injected_time_tests()
{
    reset();
    TestImu imu;
    CHECK(imu.init() == ESP_OK);

    const imu::Vec3 accel{0.0f, G, 0.0f};
    const imu::Vec3 gyro{0.0f, 0.0f, 1.0f};
    FakeMotion::next = imu::MotionSample{accel, gyro};

    filter::GravityEstimator expected(0.5f);
    expected.update(accel, gyro, 0.0f);
    expected.update(accel, gyro, 0.02f);

    imu.step(100ms);
    imu.step(120ms);
    const auto state = imu.latest();
    CHECK(state.has_value());
    CHECK(near(state->up.x, expected.value().x));
    CHECK(near(state->up.y, expected.value().y));
    CHECK(near(state->up.z, expected.value().z));
}
} // namespace

int main()
{
    init_tests();
    publish_tests();
    read_failure_tests();
    magnetic_divider_tests();
    injected_time_tests();
    std::puts("imu module tests passed");
    return EXIT_SUCCESS;
}
