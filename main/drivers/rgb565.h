#pragma once

#include <algorithm>
#include <cstdint>
#include <span>

namespace rgb565
{
inline void fill(std::span<std::uint16_t> pixels, std::uint16_t color)
{
    std::fill(pixels.begin(), pixels.end(), color);
}
}
