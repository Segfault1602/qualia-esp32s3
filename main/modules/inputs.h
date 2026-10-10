#pragma once

#include <array>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>

#include <esp_check.h>
#include <esp_err.h>
#include <esp_log.h>

namespace modules
{
enum class Input : std::uint8_t
{
    Up,
    Down,
};

/**
 * @brief A static-only source of raw button levels. level() reads one input without blocking beyond bounded I/O.
 */
template <typename T>
concept ButtonSource = requires(Input which) {
    { T::is_initialized() } -> std::same_as<bool>;
    { T::level(which) } -> std::same_as<std::expected<bool, esp_err_t>>;
};

/**
 * @brief Polls every input of Source once per step and reports debounced edges.
 *
 * An input reports an edge once, when its level has gone from high to low and then stayed low for seven steps.
 * A failed read skips that step; it is logged once until a step succeeds again.
 */
template <ButtonSource Source>
class Inputs
{
  public:
    /**
     * @brief Intended step period; the debounce window is seven periods.
     */
    static constexpr std::chrono::milliseconds PERIOD{5};

    using Callback = void (*)(Input which);

    /**
     * @param on_edge Runs inside step().
     */
    explicit constexpr Inputs(Callback on_edge) : on_edge_(on_edge)
    {
    }
    Inputs(const Inputs&) = delete;
    Inputs& operator=(const Inputs&) = delete;

    /**
     * @brief Requires an initialized Source.
     */
    esp_err_t init()
    {
        ESP_RETURN_ON_FALSE(Source::is_initialized(), ESP_ERR_INVALID_STATE, TAG, "Button source is not initialized");
        ESP_RETURN_ON_FALSE(on_edge_ != nullptr, ESP_ERR_INVALID_ARG, TAG, "Input callback is NULL");
        initialized_ = true;
        return ESP_OK;
    }

    bool is_initialized() const
    {
        return initialized_;
    }

    void step(std::chrono::microseconds)
    {
        std::array<bool, ALL.size()> levels{};
        for (const Input which : ALL)
        {
            const auto level = Source::level(which);
            if (!level)
            {
                if (!read_error_reported_)
                {
                    ESP_LOGW(TAG, "Button read failed: %s; further failures are suppressed until it recovers",
                             esp_err_to_name(level.error()));
                    read_error_reported_ = true;
                }
                return;
            }
            levels[static_cast<std::size_t>(which)] = *level;
        }
        read_error_reported_ = false;

        for (const Input which : ALL)
        {
            if (debounce(which, levels[static_cast<std::size_t>(which)]))
            {
                on_edge_(which);
            }
        }
    }

  private:
    static constexpr const char* TAG = "inputs";
    static constexpr std::array<Input, 2> ALL{Input::Up, Input::Down};
    static constexpr std::uint8_t EDGE_PATTERN = 0x80;

    bool debounce(Input which, bool level)
    {
        std::uint8_t& history = history_[static_cast<std::size_t>(which)];
        history = static_cast<std::uint8_t>((history << 1) | (level ? 1 : 0));
        return history == EDGE_PATTERN;
    }

    Callback on_edge_;
    bool initialized_ = false;
    bool read_error_reported_ = false;
    std::array<std::uint8_t, ALL.size()> history_{};
};
} // namespace modules
