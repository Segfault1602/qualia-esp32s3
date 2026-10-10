#include "bounce_midi.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <numbers>

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
auto map(double angle, float speed = 3.0f)
{
    return midi::map_impact(static_cast<float>(std::sin(angle)), static_cast<float>(std::cos(angle)), speed,
                            app::BOUNCE_SCALE, app::FULL_MIDI_VELOCITY_SPEED);
}
} // namespace

int main()
{
    constexpr double turn = 2.0 * std::numbers::pi;
    constexpr double sector = turn / app::BOUNCE_SCALE.size();
    for (std::size_t i = 0; i < app::BOUNCE_SCALE.size(); ++i)
    {
        const auto note = map((i + 0.5) * sector);
        CHECK(note && note->pitch == app::BOUNCE_SCALE[i] && note->velocity == 127);
        const auto before = map(i * sector - 0.0001);
        const auto after = map(i * sector + 0.0001);
        CHECK(before && before->pitch == app::BOUNCE_SCALE[(i + 14) % 15]);
        CHECK(after && after->pitch == app::BOUNCE_SCALE[i]);
    }
    CHECK(map(0)->pitch == 48);
    CHECK(map(turn / 4)->pitch == 53);
    CHECK(map(turn / 2)->pitch == 60);
    CHECK(map(3 * turn / 4)->pitch == 67);
    CHECK(map(-0.0001)->pitch == 72);
    CHECK(map(0, 1.5f)->velocity == 64);
    CHECK(map(0, 0.001f)->velocity == 1);
    CHECK(map(0, 1000.0f)->velocity == 127);
    CHECK(map(0, std::numeric_limits<float>::max())->velocity == 127);
    CHECK(!map(0, 0));
    CHECK(!map(0, -1));
    CHECK(!map(0, std::numeric_limits<float>::infinity()));
    CHECK(!midi::map_impact(0, 0, 1, app::BOUNCE_SCALE, 3));
    CHECK(!midi::map_impact(std::numeric_limits<float>::quiet_NaN(), 1, 1, app::BOUNCE_SCALE, 3));
    CHECK(!midi::map_impact(0, 1, 1, app::BOUNCE_SCALE, 0));
    CHECK(!midi::map_impact(0, 1, 1, {}, 3));
    constexpr std::array<std::uint8_t, 1> invalid_scale{128};
    CHECK(!midi::map_impact(0, 1, 1, invalid_scale, 3));
    const auto bottom = app::bounce_note({{physics::BORDER_CENTER.x, 0.0f}, 1.5f});
    CHECK(bottom && bottom->pitch == 60 && bottom->velocity == 64);
    return EXIT_SUCCESS;
}
