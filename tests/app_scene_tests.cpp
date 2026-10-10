#include "ball.h"
#include "imu_overlay.h"
#include "lcd_config.h"
#include "simulation.h"
#include "tilt.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string_view>
#include <type_traits>
#include <vector>

#include <lvgl.h>

#if CONFIG_QUALIA_MODE_TRACE
#include <esp_app_trace.h>
#include <esp_timer.h>
#endif

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                                       \
            std::exit(EXIT_FAILURE);                                                                                   \
        }                                                                                                              \
    } while (false)

namespace
{
using namespace std::chrono_literals;
constexpr float G = 9.80665f;
std::vector<physics::Bounce> bounces;

void record_bounce(const physics::Bounce& bounce)
{
    bounces.push_back(bounce);
}

void empty()
{
    CHECK(host_lvgl::objects.empty());
    CHECK(host_lvgl::live_buffers == 0);
}

display::Point expected_position(int steps)
{
    const auto world = physics::create_world();
    CHECK(b2World_IsValid(world));
    CHECK(b2Body_IsValid(physics::create_border(world)));
    const auto body = physics::create_ball(world, {0.3f, 0.8f});
    CHECK(b2Body_IsValid(body));
    for (int i = 0; i < steps; ++i)
    {
        physics::step(world);
    }
    const auto position = b2Body_GetPosition(body);
    const auto point = display::world_to_panel(position.x, position.y, drivers::lcd::WIDTH, drivers::lcd::HEIGHT);
    b2DestroyWorld(world);
    return point;
}

void ball_tests()
{
    static_assert(!std::is_copy_constructible_v<app::Ball>);
    static_assert(!std::is_move_constructible_v<app::Ball>);
    const auto world = physics::create_world();
    b2BodyId body = b2_nullBodyId;
    {
        app::Ball ball;
        CHECK(ball.draw() == ESP_ERR_INVALID_STATE);
        CHECK(ball.create(b2_nullWorldId, {0, 0}) == ESP_ERR_INVALID_ARG);
        CHECK(ball.create(world, {std::numeric_limits<float>::quiet_NaN(), 0}) == ESP_ERR_INVALID_ARG);
        host_lvgl::allocation_budget = 1;
        CHECK(ball.create(world, {0.3f, 0.8f}) == ESP_ERR_NO_MEM);
        empty();
        host_lvgl::allocation_budget = -1;
        CHECK(ball.create(world, {0.3f, 0.8f}) == ESP_OK);
        CHECK(ball.create(world, {0.3f, 0.8f}) == ESP_ERR_INVALID_STATE);
        body = ball.body();
        CHECK(b2Body_IsValid(body));
        b2Body_SetTransform(body, {-0.25f, 1.25f}, b2Rot_identity);
        CHECK(ball.draw() == ESP_OK);
        const auto* canvas = host_lvgl::find(lv_obj_t::Kind::Canvas);
        CHECK(canvas->x == -145 && canvas->y == -145);
        ball.destroy();
        ball.destroy();
        CHECK(!b2Body_IsValid(body));
        CHECK(ball.draw() == ESP_ERR_INVALID_STATE);
        CHECK(ball.create(world, {0.3f, 0.8f}) == ESP_OK);
        body = ball.body();
    }
    CHECK(!b2Body_IsValid(body));
    b2DestroyWorld(world);
    empty();
}

void initialization_tests()
{
    static_assert(!std::is_copy_constructible_v<app::Simulation>);
    static_assert(!std::is_move_constructible_v<app::Simulation>);
    for (int budget = 0; budget < 3; ++budget)
    {
        app::Simulation simulation;
        CHECK(simulation.step(0us) == ESP_ERR_INVALID_STATE);
        host_lvgl::allocation_budget = budget;
        CHECK(simulation.init() == ESP_ERR_NO_MEM);
        CHECK(!simulation.is_initialized());
        empty();
        simulation.destroy();
        host_lvgl::allocation_budget = -1;
        CHECK(simulation.init() == ESP_OK);
        CHECK(simulation.is_initialized());
        CHECK(simulation.init() == ESP_ERR_INVALID_STATE);
        simulation.destroy();
        simulation.destroy();
        CHECK(!simulation.is_initialized());
        empty();
    }
    for (int i = 0; i < 200; ++i)
    {
        app::Simulation simulation;
        CHECK(simulation.init() == ESP_OK);
        CHECK(simulation.step(0us) == ESP_OK);
    }
    empty();
}

void clock_tests()
{
    const auto first = expected_position(1);
    const auto second = expected_position(2);
    const auto sixth = expected_position(6);
    const auto seventh = expected_position(7);
    app::Simulation simulation;
    CHECK(simulation.init() == ESP_OK);
    const auto* canvas = host_lvgl::find(lv_obj_t::Kind::Canvas);
    CHECK(canvas->moves == 1);
    CHECK(simulation.step(-1us) == ESP_ERR_INVALID_ARG);
    CHECK(simulation.step(1s) == ESP_OK);
    CHECK(simulation.step(1s + 16'666us) == ESP_OK);
    CHECK(canvas->moves == 1);
    CHECK(simulation.step(1s + 16'667us) == ESP_OK);
    CHECK(canvas->moves == 2);
    CHECK(canvas->x + 25 == first.x && canvas->y + 25 == first.y);
    CHECK(simulation.step(1s + 16'667us) == ESP_OK);
    CHECK(canvas->moves == 2);
    CHECK(simulation.step(1s + 16'666us) == ESP_ERR_INVALID_ARG);
    CHECK(simulation.step(1s + 33'334us) == ESP_OK);
    CHECK(canvas->x + 25 == second.x && canvas->y + 25 == second.y);
    CHECK(simulation.step(2s + 33'334us) == ESP_OK);
    CHECK(canvas->moves == 4);
    CHECK(canvas->x + 25 == sixth.x && canvas->y + 25 == sixth.y);
    CHECK(simulation.step(2s + 50'000us) == ESP_OK);
    CHECK(canvas->x + 25 == seventh.x && canvas->y + 25 == seventh.y);
    simulation.destroy();
    CHECK(simulation.init() == ESP_OK);
    canvas = host_lvgl::find(lv_obj_t::Kind::Canvas);
    CHECK(simulation.step(0us) == ESP_OK);
    CHECK(simulation.step(16'666us) == ESP_OK);
    CHECK(canvas->moves == 1);
}

void physics_tests()
{
    bounces.clear();
    app::Simulation simulation;
    CHECK(simulation.init(record_bounce) == ESP_OK);
    CHECK(simulation.step(0us) == ESP_OK);
    const auto* canvas = host_lvgl::find(lv_obj_t::Kind::Canvas);
    for (int step = 1; step <= 3600; ++step)
    {
        const auto count = bounces.size();
        CHECK(simulation.step(std::chrono::microseconds{(step * 1'000'000LL + 59) / 60}) == ESP_OK);
        CHECK(bounces.size() <= count + 1);
    }
    CHECK(bounces.size() >= 2);
    for (const auto& bounce : bounces)
    {
        CHECK(bounce.approach_speed > 0.2f);
        CHECK(b2Distance(bounce.point, physics::BORDER_CENTER) > physics::BORDER_RADIUS - physics::BALL_RADIUS - 0.01f);
    }
    CHECK(std::abs(canvas->x + 25 - 240) < 5);
    CHECK(canvas->y + 25 > 450);
    for (int step = 3601; step <= 4200; ++step)
    {
        CHECK(simulation.step(std::chrono::microseconds{(step * 1'000'000LL + 59) / 60}, imu::Vec3{G, 0, 0}) == ESP_OK);
    }
    CHECK(canvas->x + 25 > 430);
    CHECK(std::abs(canvas->y + 25 - 240) < 24);
    for (int step = 4201; step <= 4260; ++step)
    {
        CHECK(simulation.step(std::chrono::microseconds{(step * 1'000'000LL + 59) / 60}) == ESP_OK);
    }
    CHECK(canvas->x + 25 > 430);
}

void overlay_tests()
{
    static_assert(!std::is_copy_constructible_v<app::ImuOverlay>);
    app::ImuOverlay overlay;
    CHECK(overlay.step(0us, std::nullopt) == ESP_ERR_INVALID_STATE);
    host_lvgl::allocation_budget = 0;
    CHECK(overlay.init() == ESP_ERR_NO_MEM);
    CHECK(!overlay.is_initialized());
    host_lvgl::allocation_budget = -1;
    CHECK(overlay.init() == ESP_OK);
    CHECK(overlay.init() == ESP_ERR_INVALID_STATE);
    const auto* label = host_lvgl::find(lv_obj_t::Kind::Label);
    const modules::ImuState snapshot{{1, 2, 3}, {{}, {4, 5, 6}}, std::nullopt};
    CHECK(overlay.step(249ms, snapshot) == ESP_OK);
    CHECK(label->text == "IMU: no data");
    CHECK(overlay.step(250ms, std::nullopt) == ESP_OK);
    CHECK(overlay.step(499ms, snapshot) == ESP_OK);
    CHECK(label->text == "IMU: no data");
    CHECK(overlay.step(500ms, snapshot) == ESP_OK);
    CHECK(label->text == "a  +1.00  +2.00  +3.00\ng  +4.00  +5.00  +6.00\nm   +0.0   +0.0   +0.0");
    CHECK(overlay.step(499ms, snapshot) == ESP_ERR_INVALID_ARG);
    overlay.destroy();
    overlay.destroy();
    CHECK(!overlay.is_initialized());
    empty();
    CHECK(overlay.init() == ESP_OK);
    CHECK(overlay.step(250ms, snapshot) == ESP_OK);
    overlay.destroy();
}

#if CONFIG_QUALIA_MODE_TRACE
int trace_errors = 0;

void observe_log(const char*, const char* format)
{
    if (std::string_view{format}.starts_with("Trace write failed"))
    {
        ++trace_errors;
    }
}

void trace_tests()
{
    app::Simulation simulation;
    CHECK(simulation.init() == ESP_OK);
    CHECK(simulation.step(0us) == ESP_OK);
    host_trace::connected = true;
    host_trace::records.clear();
    host_timer::now = 1000;
    CHECK(simulation.step(33'334us) == ESP_OK);
    CHECK(host_trace::records.size() == 2);
    CHECK(host_trace::records[0] == "[1000] - update_simulation: delta=100\n");
    CHECK(host_trace::records[1] == "[1200] - update_simulation: delta=100\n");
    host_log_observer = observe_log;
    host_trace::write_result = ESP_FAIL;
    CHECK(simulation.step(66'667us) == ESP_OK);
    CHECK(trace_errors == 1);
    host_trace::connected = false;
    const int writes = host_trace::writes;
    CHECK(simulation.step(83'334us) == ESP_OK);
    CHECK(host_trace::writes == writes);
    host_trace::connected = true;
    CHECK(simulation.step(100'000us) == ESP_OK);
    CHECK(trace_errors == 2);
    host_trace::write_result = ESP_OK;
    host_timer::now = std::numeric_limits<std::int64_t>::max() - 200;
    CHECK(simulation.step(116'667us) == ESP_OK);
    CHECK(host_trace::records.back().size() < 96);
    CHECK(host_trace::records.back().ends_with("] - update_simulation: delta=100\n"));
    host_trace::connected = false;
    host_log_observer = nullptr;
}
#endif
} // namespace

int main()
{
    ball_tests();
    initialization_tests();
    clock_tests();
    empty();
    physics_tests();
    empty();
    overlay_tests();
#if CONFIG_QUALIA_MODE_TRACE
    trace_tests();
#endif
    empty();
    return EXIT_SUCCESS;
}
