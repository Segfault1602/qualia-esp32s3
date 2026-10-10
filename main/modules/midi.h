#pragma once

#include <array>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <expected>
#include <optional>

#include <esp_check.h>
#include <esp_err.h>
#include <esp_log.h>

#include "midi_bootloader.h"
#include "midi_note.h"
#include "midi_usb.h"

namespace modules
{
template <typename T>
concept MidiTransport = requires(const drivers::MidiUsb::Packet& packet) {
    { T::init() } -> std::same_as<esp_err_t>;
    { T::connected() } -> std::same_as<bool>;
    { T::read_packet() } -> std::same_as<std::optional<drivers::MidiUsb::Packet>>;
    { T::write_packet(packet) } -> std::same_as<std::expected<void, esp_err_t>>;
};

/**
 * @brief Passive USB MIDI output and input logger. All methods run on the MIDI task.
 */
template <MidiTransport Transport>
class MidiController
{
  public:
    static constexpr std::chrono::milliseconds PERIOD{1};
    static constexpr std::chrono::milliseconds NOTE_DURATION{100};

    MidiController() = default;
    MidiController(const MidiController&) = delete;
    MidiController& operator=(const MidiController&) = delete;

    esp_err_t init()
    {
        ESP_RETURN_ON_FALSE(!initialized_, ESP_ERR_INVALID_STATE, TAG, "MIDI is already initialized");
        ESP_RETURN_ON_ERROR(Transport::init(), TAG, "USB MIDI initialization failed");
        initialized_ = true;
        return ESP_OK;
    }

    bool is_initialized() const
    {
        return initialized_;
    }

    bool ready_for_notes() const
    {
        return initialized_ && connected_ && !cleanup_ && !bootloader_requested_;
    }

    bool can_submit() const
    {
        return ready_for_notes() && !pending_;
    }

    bool bootloader_requested() const
    {
        return bootloader_requested_;
    }

    bool bootloader_ready() const
    {
        return bootloader_requested_ && !cleanup_;
    }

    esp_err_t submit(midi::Note note)
    {
        ESP_RETURN_ON_FALSE(note.pitch <= 127 && note.velocity > 0 && note.velocity <= 127, ESP_ERR_INVALID_ARG, TAG,
                            "Invalid MIDI note");
        ESP_RETURN_ON_FALSE(can_submit(), ESP_ERR_INVALID_STATE, TAG, "MIDI cannot accept a note");
        pending_ = note;
        return ESP_OK;
    }

    void step(std::chrono::microseconds now)
    {
        if (!initialized_)
        {
            ESP_LOGE(TAG, "Cannot step uninitialized MIDI");
            return;
        }
        if (!Transport::connected())
        {
            if (connected_)
            {
                ESP_LOGW(TAG, "USB MIDI disconnected; discarding unsent bounces");
            }
            connected_ = false;
            pending_.reset();
            bootloader_command_.reset();
            cleanup_ = true;
            return;
        }
        if (!connected_)
        {
            ESP_LOGI(TAG, "USB MIDI connected");
            connected_ = true;
        }
        service_output(now);
        for (int i = 0; i < 16; ++i)
        {
            const auto packet = Transport::read_packet();
            if (!packet)
            {
                break;
            }
            const auto& data = *packet;
            const long long now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
            ESP_LOGI(TAG, "Read - Time (ms since boot): %lld, Data: %02hhX %02hhX %02hhX %02hhX", now_ms, data[0],
                     data[1], data[2], data[3]);
            if (!bootloader_requested_ && bootloader_command_.feed(data, now))
            {
                ESP_LOGI(TAG, "Bootloader SysEx accepted");
                bootloader_requested_ = true;
                pending_.reset();
                cleanup_ = true;
                break;
            }
        }
    }

  private:
    struct ActiveNote
    {
        std::chrono::microseconds deadline{};
        bool active = false;
    };

    bool write(const drivers::MidiUsb::Packet& packet)
    {
        const auto result = Transport::write_packet(packet);
        if (!result)
        {
            if (!write_error_reported_)
            {
                ESP_LOGW(TAG, "MIDI write failed: %s; retrying until recovery", esp_err_to_name(result.error()));
                write_error_reported_ = true;
            }
            return false;
        }
        write_error_reported_ = false;
        return true;
    }

    void service_output(std::chrono::microseconds now)
    {
        for (std::size_t pitch = 0; pitch < active_.size(); ++pitch)
        {
            auto& note = active_[pitch];
            if (note.active && (cleanup_ || now >= note.deadline))
            {
                if (!write({0x08, 0x80, static_cast<std::uint8_t>(pitch), 0}))
                {
                    return;
                }
                note.active = false;
            }
        }
        cleanup_ = false;
        if (!pending_)
        {
            return;
        }
        auto& active = active_[pending_->pitch];
        if (active.active)
        {
            if (!write({0x08, 0x80, pending_->pitch, 0}))
            {
                return;
            }
            active.active = false;
        }
        if (!write({0x09, 0x90, pending_->pitch, pending_->velocity}))
        {
            return;
        }
        active = {now + NOTE_DURATION, true};
        pending_.reset();
    }

    static constexpr const char* TAG = "MIDI";
    bool initialized_ = false;
    bool connected_ = false;
    bool cleanup_ = false;
    bool write_error_reported_ = false;
    bool bootloader_requested_ = false;
    midi::BootloaderCommand bootloader_command_;
    std::optional<midi::Note> pending_;
    std::array<ActiveNote, 128> active_{};
};

using Midi = MidiController<drivers::MidiUsb>;
} // namespace modules
