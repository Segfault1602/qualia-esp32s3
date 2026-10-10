#include "gui/ball_view.h"
#include "gui/border_view.h"
#include "gui/imu_readout.h"

#include <cstdio>
#include <cstdlib>
#include <limits>
#include <type_traits>

#include <lvgl.h>

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
void empty()
{
    CHECK(host_lvgl::objects.empty());
    CHECK(host_lvgl::live_buffers == 0);
}

void ball_tests()
{
    static_assert(!std::is_copy_constructible_v<gui::BallView>);
    static_assert(!std::is_move_constructible_v<gui::BallView>);
    for (int budget = 0; budget < 2; ++budget)
    {
        host_lvgl::allocation_budget = budget;
        gui::BallView view;
        CHECK(view.create(41) == ESP_ERR_NO_MEM);
        empty();
        view.destroy();
        host_lvgl::allocation_budget = -1;
        CHECK(view.create(41) == ESP_OK);
    }
    empty();
    const int objects_before = host_lvgl::object_deletions;
    const int buffers_before = host_lvgl::buffer_deletions;
    {
        gui::BallView view;
        CHECK(view.move_to({0, 0}) == ESP_ERR_INVALID_STATE);
        CHECK(view.create(0) == ESP_ERR_INVALID_ARG);
        CHECK(view.create(51) == ESP_ERR_INVALID_ARG);
        CHECK(view.create(41) == ESP_OK);
        CHECK(view.create(41) == ESP_ERR_INVALID_STATE);
        const auto* canvas = host_lvgl::find(lv_obj_t::Kind::Canvas);
        CHECK(canvas != nullptr);
        CHECK(canvas->rectangle.x2 - canvas->rectangle.x1 + 1 == 41);
        CHECK(view.move_to({0, 0}) == ESP_OK);
        CHECK(canvas->x == -25 && canvas->y == -25);
        view.destroy();
        view.destroy();
        CHECK(view.move_to({0, 0}) == ESP_ERR_INVALID_STATE);
    }
    CHECK(host_lvgl::object_deletions == objects_before + 1);
    CHECK(host_lvgl::buffer_deletions == buffers_before + 1);
    empty();
}

void border_tests()
{
    static_assert(!std::is_copy_constructible_v<gui::BorderView>);
    {
        gui::BorderView view;
        CHECK(view.create({240, 240}, 0) == ESP_ERR_INVALID_ARG);
        CHECK(view.create({240, 240}, std::numeric_limits<int>::max()) == ESP_ERR_INVALID_ARG);
        host_lvgl::allocation_budget = 0;
        CHECK(view.create({240, 240}, 234) == ESP_ERR_NO_MEM);
        host_lvgl::allocation_budget = -1;
        CHECK(view.create({240, 240}, 234) == ESP_OK);
        CHECK(view.create({240, 240}, 234) == ESP_ERR_INVALID_STATE);
        const auto* ring = host_lvgl::find(lv_obj_t::Kind::Ring);
        CHECK(ring->x == 4 && ring->y == 4);
        CHECK(ring->width == 472 && ring->height == 472);
        view.destroy();
        view.destroy();
    }
    empty();
    {
        gui::BorderView view;
        CHECK(view.create({240, 240}, 234) == ESP_OK);
    }
    empty();
}

void readout_tests()
{
    static_assert(!std::is_copy_constructible_v<gui::ImuReadout>);
    {
        gui::ImuReadout view;
        CHECK(view.show({}, {}, {}) == ESP_ERR_INVALID_STATE);
        host_lvgl::allocation_budget = 0;
        CHECK(view.create() == ESP_ERR_NO_MEM);
        host_lvgl::allocation_budget = -1;
        CHECK(view.create() == ESP_OK);
        CHECK(view.create() == ESP_ERR_INVALID_STATE);
        const auto* label = host_lvgl::find(lv_obj_t::Kind::Label);
        CHECK(label->text == "IMU: no data");
        CHECK(view.show({1, 2, 3}, {4, 5, 6}, {7, 8, 9}) == ESP_OK);
        CHECK(label->text == "a  +1.00  +2.00  +3.00\ng  +4.00  +5.00  +6.00\nm   +7.0   +8.0   +9.0");
        const auto previous = label->text;
        const float huge = std::numeric_limits<float>::max();
        CHECK(view.show({huge, huge, huge}, {huge, huge, huge}, {huge, huge, huge}) == ESP_ERR_INVALID_SIZE);
        CHECK(label->text == previous);
        view.destroy();
        view.destroy();
    }
    empty();
    {
        gui::ImuReadout view;
        CHECK(view.create() == ESP_OK);
    }
    empty();
}
} // namespace

int main()
{
    ball_tests();
    border_tests();
    readout_tests();
    return EXIT_SUCCESS;
}
