#pragma once

#include <array>
#include <cstdint>
#include <expected>
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

    static bool connected();

    /**
     * @brief Submit a packet without waiting. INVALID_STATE means offline; TIMEOUT means a full TX buffer.
     */
    static std::expected<void, esp_err_t> write_packet(const Packet& packet);

    /**
     * @brief Disconnect USB and reset into the ESP32-S3 ROM downloader. Returns only on failure.
     */
    static esp_err_t enter_bootloader();
};
} // namespace drivers
