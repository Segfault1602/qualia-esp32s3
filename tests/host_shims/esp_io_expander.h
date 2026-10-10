#pragma once

#include <cstdint>

#include "esp_err.h"

struct esp_io_expander_s;
using esp_io_expander_handle_t = esp_io_expander_s*;
using esp_io_expander_pin_num_t = std::uint32_t;
inline constexpr esp_io_expander_pin_num_t IO_EXPANDER_PIN_NUM_0 = 1U << 0;
inline constexpr esp_io_expander_pin_num_t IO_EXPANDER_PIN_NUM_1 = 1U << 1;
inline constexpr esp_io_expander_pin_num_t IO_EXPANDER_PIN_NUM_2 = 1U << 2;
inline constexpr esp_io_expander_pin_num_t IO_EXPANDER_PIN_NUM_4 = 1U << 4;
inline constexpr esp_io_expander_pin_num_t IO_EXPANDER_PIN_NUM_5 = 1U << 5;
inline constexpr esp_io_expander_pin_num_t IO_EXPANDER_PIN_NUM_6 = 1U << 6;
inline constexpr esp_io_expander_pin_num_t IO_EXPANDER_PIN_NUM_7 = 1U << 7;
enum esp_io_expander_dir_t
{
    IO_EXPANDER_INPUT,
    IO_EXPANDER_OUTPUT
};

esp_err_t esp_io_expander_set_dir(esp_io_expander_handle_t handle, std::uint32_t pins, esp_io_expander_dir_t direction);
esp_err_t esp_io_expander_set_level(esp_io_expander_handle_t handle, std::uint32_t pins, std::uint32_t level);
esp_err_t esp_io_expander_print_state(esp_io_expander_handle_t handle);
esp_err_t esp_io_expander_del(esp_io_expander_handle_t handle);
