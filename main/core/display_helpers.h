#pragma once

namespace display
{
struct Point
{
    int x;
    int y;
};

constexpr Point world_to_panel(float x, float y, int width, int height)
{
    return {static_cast<int>(x * width), static_cast<int>((1.0f - y) * height)};
}

constexpr Point centered_position(Point center, int width, int height)
{
    return {center.x - width / 2, center.y - height / 2};
}
}
