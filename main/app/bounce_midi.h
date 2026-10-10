#pragma once

#include <array>

#include "midi_note.h"
#include "physics_world.h"

namespace app
{
constexpr std::array<std::uint8_t, 15> BOUNCE_SCALE{48, 50, 52, 53, 55, 57, 59, 60, 62, 64, 65, 67, 69, 71, 72};
constexpr float FULL_MIDI_VELOCITY_SPEED = 3.0f;

inline std::optional<midi::Note> bounce_note(const physics::Bounce& bounce)
{
    return midi::map_impact(bounce.point.x - physics::BORDER_CENTER.x, bounce.point.y - physics::BORDER_CENTER.y,
                            bounce.approach_speed, BOUNCE_SCALE, FULL_MIDI_VELOCITY_SPEED);
}
} // namespace app
