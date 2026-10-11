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
concept MidiTransport = requires(const midi::Message& message, midi::State state) {
    { T::init() } -> std::same_as<esp_err_t>;
    { T::state() } -> std::same_as<midi::State>;
    { T::read_message() } -> std::same_as<std::expected<std::optional<midi::Message>, esp_err_t>>;
    { T::write_message(message, state) } -> std::same_as<std::expected<void, esp_err_t>>;
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

    std::uint32_t session_generation() const
    {
        return state_.generation;
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
        ESP_RETURN_ON_FALSE(midi::valid(note), ESP_ERR_INVALID_ARG, TAG, "Invalid MIDI note");
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
        const auto state = Transport::state();
        if (state != state_)
        {
            abandon_operation();
            pending_.reset();
            bootloader_command_.reset();
            cleanup_ = true;
            state_ = state;
        }
        if (!state.connected)
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
            const auto received = Transport::read_message();
            if (Transport::state() != state_)
            {
                bootloader_command_.reset();
                break;
            }
            if (!received)
            {
                bootloader_command_.reset();
                if (!read_error_reported_)
                {
                    ESP_LOGW(TAG, "MIDI input failed: %s; discarding partial command",
                             esp_err_to_name(received.error()));
                    read_error_reported_ = true;
                }
                break;
            }
            read_error_reported_ = false;
            if (!*received)
            {
                break;
            }
            const auto& message = **received;
            const long long now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
            const bool log = now >= next_input_log_;
            if (!log)
            {
                ++suppressed_logs_;
            }
            else
            {
                next_input_log_ = now + std::chrono::milliseconds(50);
                if (suppressed_logs_ != 0)
                {
                    ESP_LOGI(TAG, "MIDI input log rate limited: %lu messages omitted",
                             static_cast<unsigned long>(suppressed_logs_));
                    suppressed_logs_ = 0;
                }
            }
            if (log && message.ump)
            {
                ESP_LOGI(TAG, "Read UMP - Time (ms): %lld, Protocol: %s, Words: %u, %08lX %08lX %08lX %08lX", now_ms,
                         state_.format == midi::Format::UmpMidi2 ? "2.0" : "1.0", static_cast<unsigned>(message.size),
                         static_cast<unsigned long>(message.words[0]), static_cast<unsigned long>(message.words[1]),
                         static_cast<unsigned long>(message.words[2]), static_cast<unsigned long>(message.words[3]));
            }
            else if (log)
            {
                const auto data = message.packet();
                ESP_LOGI(TAG, "Read - Time (ms since boot): %lld, Data: %02hhX %02hhX %02hhX %02hhX", now_ms, data[0],
                         data[1], data[2], data[3]);
            }
            if (!bootloader_requested_ && bootloader_command_.feed_message(message, now))
            {
                ESP_LOGI(TAG, "Bootloader SysEx accepted");
                bootloader_requested_ = true;
                abandon_operation();
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

    struct Operation
    {
        midi::Note note;
        bool on;
    };

    void abandon_operation()
    {
        if (operation_ && operation_->on)
        {
            active_[operation_->note.pitch].active = true;
        }
        operation_.reset();
    }

    bool write(const midi::Message& message)
    {
        const auto result = Transport::write_message(message, state_);
        if (!result)
        {
            if (result.error() == ESP_ERR_NOT_FINISHED)
            {
                return false;
            }
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
        for (std::size_t iteration = 0; iteration < active_.size() + 2; ++iteration)
        {
            if (!operation_)
            {
                for (std::size_t pitch = 0; pitch < active_.size(); ++pitch)
                {
                    if (active_[pitch].active && (cleanup_ || now >= active_[pitch].deadline))
                    {
                        operation_ = Operation{{static_cast<std::uint8_t>(pitch), 1.0}, false};
                        break;
                    }
                }
            }
            if (!operation_)
            {
                cleanup_ = false;
                if (!pending_)
                {
                    return;
                }
                operation_ = Operation{*pending_, !active_[pending_->pitch].active};
            }
            if (!write(midi::note_message(operation_->note, operation_->on, state_.format)))
            {
                return;
            }
            auto& active = active_[operation_->note.pitch];
            if (operation_->on)
            {
                active = {now + NOTE_DURATION, true};
                pending_.reset();
            }
            else
            {
                active.active = false;
            }
            operation_.reset();
        }
    }

    static constexpr const char* TAG = "MIDI";
    bool initialized_ = false;
    bool connected_ = false;
    bool cleanup_ = false;
    bool write_error_reported_ = false;
    bool read_error_reported_ = false;
    bool bootloader_requested_ = false;
    std::chrono::microseconds next_input_log_{};
    std::uint32_t suppressed_logs_ = 0;
    midi::BootloaderCommand bootloader_command_;
    std::optional<midi::Note> pending_;
    std::optional<Operation> operation_;
    midi::State state_{};
    std::array<ActiveNote, 128> active_{};
};

using Midi = MidiController<drivers::MidiUsb>;
} // namespace modules
