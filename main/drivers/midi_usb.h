#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <optional>

#include <esp_err.h>

#include "midi_message.h"

namespace drivers
{
/**
 * @brief USB MIDI 2.0 device with MIDI 1.0 fallback on the native USB port, through TinyUSB.
 *
 * init() also starts TinyUSB's own device task. Alternate setting 0 carries USB MIDI 1.0
 * event packets; alternate setting 1 carries UMP.
 */
class MidiUsb
{
  public:
    MidiUsb() = delete;

    /**
     * @brief Install USB MIDI 2.0 with MIDI 1.0 fallback.
     */
    static esp_err_t init();
    static bool is_initialized();

    /**
     * @brief One 4-byte USB MIDI event packet.
     */
    using Packet = midi::UsbPacket;

    static midi::State state();
    static std::expected<std::optional<midi::Message>, esp_err_t> read_message();

    /**
     * @brief Nonblocking complete-message submission in the supplied session.
     *
     * Repeat the identical message until success. NOT_FINISHED means pending;
     * TIMEOUT means TX pressure; INVALID_STATE means the session is obsolete/offline.
     */
    static std::expected<void, esp_err_t> write_message(const midi::Message& message, midi::State state);

    static bool connected();

    /**
     * @brief Disconnect USB and reset into the ESP32-S3 ROM downloader. Returns only on failure.
     */
    static esp_err_t enter_bootloader();
};
} // namespace drivers
