#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include <esp_err.h>

namespace drivers
{
/**
 * @brief USB MIDI device on the native USB port, through TinyUSB.
 *
 * init() also starts TinyUSB's own device task.
 */
class MidiUsb
{
  public:
    MidiUsb() = delete;

    /**
     * @brief Install the TinyUSB driver with a single MIDI interface.
     */
    static esp_err_t init();
    static bool is_initialized();

    /**
     * @brief One 4-byte USB MIDI event packet.
     */
    using Packet = std::array<std::uint8_t, 4>;

    /**
     * @brief Next received packet, or nullopt when none is waiting (or before init()). Does not block.
     */
    static std::optional<Packet> read_packet();
};
} // namespace drivers
