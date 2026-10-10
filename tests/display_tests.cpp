#include "display_helpers.h"
#include "rgb565.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    std::exit(EXIT_FAILURE); \
} } while (false)

int main()
{
    constexpr std::size_t width = 480;
    constexpr std::size_t height = 480;
    constexpr std::size_t guard = 8;
    constexpr std::uint16_t sentinel = 0xA55A;
    constexpr std::array<std::uint16_t, 5> colors{0xFFFF, 0x0000, 0xF800, 0x07E0, 0x001F};
    for (const auto color : colors)
    {
        std::vector<std::uint16_t> storage(width * height + 2 * guard, sentinel);
        rgb565::fill({storage.data() + guard, width * height}, color);
        for (std::size_t i = 0; i < storage.size(); ++i)
        {
            const bool inside = i >= guard && i < guard + width * height;
            CHECK(storage[i] == (inside ? color : sentinel));
        }
    }
    std::array<std::uint16_t, 2> empty_guard{sentinel, sentinel};
    rgb565::fill({empty_guard.data() + 1, 0}, 0);
    CHECK(empty_guard[0] == sentinel && empty_guard[1] == sentinel);

    const auto top_left = display::world_to_panel(0, 1, width, height);
    const auto bottom_right = display::world_to_panel(1, 0, width, height);
    CHECK(top_left.x == 0 && top_left.y == 0);
    CHECK(bottom_right.x == 480 && bottom_right.y == 480);
    const auto initial = display::world_to_panel(0.5f, 0.9f, width, height);
    const auto ground = display::world_to_panel(0.5f, 0.1f, width, height);
    const auto falling = display::world_to_panel(0.5f, 0.8f, width, height);
    CHECK(initial.x == 240 && initial.y == 48);
    CHECK(ground.y == 432);
    CHECK(initial.y < falling.y && falling.y < ground.y);
    const auto origin = display::centered_position(initial, 50, 50);
    CHECK(origin.x == 215 && origin.y == 23);
    const auto negative = display::centered_position(display::world_to_panel(-0.25f, 1.25f, width, height), 50, 50);
    CHECK(negative.x == -145 && negative.y == -145);
    const auto edge = display::centered_position(top_left, 50, 50);
    CHECK(edge.x == -25 && edge.y == -25);
    const auto non_square = display::world_to_panel(0.5f, 0.25f, 640, 320);
    CHECK(non_square.x == 320 && non_square.y == 240);
    std::puts("Display helper regressions passed");
}
