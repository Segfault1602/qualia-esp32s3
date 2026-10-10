#pragma once

#include <concepts>

#include <esp_err.h>

namespace drivers
{
/**
 * @brief A board resource that is initialized once at startup through static member functions.
 *
 * init() must fail with ESP_ERR_INVALID_STATE when called twice or before the drivers it depends on.
 */
template <typename T>
concept HardwareDriver = requires {
    { T::init() } -> std::same_as<esp_err_t>;
    { T::is_initialized() } -> std::same_as<bool>;
};
} // namespace drivers
