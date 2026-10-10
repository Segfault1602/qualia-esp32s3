#pragma once

#include <chrono>
#include <concepts>

#include <esp_err.h>

namespace modules
{
/**
 * @brief A passive subsystem built on drivers. It never creates tasks; the application decides which task runs it.
 *
 * init() initializes the drivers the module owns, and is_initialized() reports whether it succeeded. step() runs
 * one iteration of the module's work and must only be called after init() succeeds, from one task at a time. It
 * does not block except on bounded driver I/O. now is the time since boot.
 */
template <typename T>
concept Module = requires(T& module, const T& const_module, std::chrono::microseconds now) {
    { module.init() } -> std::same_as<esp_err_t>;
    { const_module.is_initialized() } -> std::same_as<bool>;
    { module.step(now) } -> std::same_as<void>;
};
} // namespace modules
