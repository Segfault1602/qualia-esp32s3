#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>
#include <span>

namespace midi
{
struct Note
{
    std::uint8_t pitch;
    std::uint8_t velocity;
};

inline std::optional<Note> map_impact(float x, float y, float approach_speed, std::span<const std::uint8_t> scale,
                                      float full_velocity_speed)
{
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(approach_speed) ||
        !std::isfinite(full_velocity_speed) || (x == 0.0f && y == 0.0f) || approach_speed <= 0.0f ||
        full_velocity_speed <= 0.0f || scale.empty())
    {
        return std::nullopt;
    }
    constexpr double turn = 2.0 * std::numbers::pi;
    double angle = std::atan2(static_cast<double>(x), static_cast<double>(y));
    if (angle < 0.0)
    {
        angle += turn;
    }
    const auto index = std::min(static_cast<std::size_t>(angle / turn * scale.size()), scale.size() - 1);
    if (scale[index] > 127)
    {
        return std::nullopt;
    }
    const double fraction = std::min(static_cast<double>(approach_speed) / full_velocity_speed, 1.0);
    const auto velocity = static_cast<std::uint8_t>(std::max(1L, std::lround(127.0 * fraction)));
    return Note{scale[index], velocity};
}
} // namespace midi
