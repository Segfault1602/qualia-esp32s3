#pragma once

#include <cstddef>
#include <cstdint>

namespace drivers::lcd
{
inline constexpr int WIDTH = 480;
inline constexpr int HEIGHT = 480;
inline constexpr std::size_t PIXEL_COUNT = WIDTH * HEIGHT;
inline constexpr std::size_t FRAMEBUFFER_BYTES = PIXEL_COUNT * sizeof(std::uint16_t);
} // namespace drivers::lcd
