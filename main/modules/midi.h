#pragma once

#include <chrono>

#include <esp_err.h>

namespace modules
{
/**
 * @brief Owns drivers::MidiUsb. Each step drains incoming USB MIDI packets, logging them.
 *
 * The MIDI interface always exposes input and output ports, so incoming traffic must be read (or discarded) to keep
 * the sender from blocking.
 */
class Midi
{
  public:
    /**
     * @brief Intended step period.
     */
    static constexpr std::chrono::milliseconds PERIOD{1};

    Midi() = default;
    Midi(const Midi&) = delete;
    Midi& operator=(const Midi&) = delete;

    esp_err_t init();
    bool is_initialized() const;
    void step(std::chrono::microseconds now);

  private:
    bool initialized_ = false;
};
} // namespace modules
