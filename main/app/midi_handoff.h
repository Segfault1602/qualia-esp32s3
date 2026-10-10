#pragma once

#include <array>
#include <atomic>
#include <cstdint>

#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "midi_note.h"

namespace app
{
class MidiHandoff
{
  public:
    static constexpr UBaseType_t CAPACITY = 32;

    esp_err_t init()
    {
        if (queue_ != nullptr)
        {
            return ESP_ERR_INVALID_STATE;
        }
        queue_ = xQueueCreateStatic(CAPACITY, sizeof(Event), storage_.data(), &control_);
        return queue_ != nullptr ? ESP_OK : ESP_ERR_NO_MEM;
    }

    // Call from the consumer only. Going offline discards queued events.
    esp_err_t set_online(bool online)
    {
        if (queue_ == nullptr)
        {
            return ESP_ERR_INVALID_STATE;
        }
        const auto epoch = epoch_.load();
        if ((epoch % 2 != 0) == online)
        {
            return ESP_OK;
        }
        epoch_.fetch_add(1);
        if (!online && xQueueReset(queue_) != pdPASS)
        {
            return ESP_FAIL;
        }
        return ESP_OK;
    }

    esp_err_t publish(midi::Note note)
    {
        const auto epoch = epoch_.load();
        if (epoch % 2 == 0)
        {
            return ESP_ERR_INVALID_STATE;
        }
        const Event event{note, epoch};
        return xQueueSend(queue_, &event, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
    }

    std::optional<midi::Note> receive()
    {
        Event event{};
        for (UBaseType_t i = 0; i < CAPACITY && xQueueReceive(queue_, &event, 0) == pdPASS; ++i)
        {
            const auto epoch = epoch_.load();
            if (epoch % 2 != 0 && event.epoch == epoch)
            {
                return event.note;
            }
        }
        return std::nullopt;
    }

  private:
    struct Event
    {
        midi::Note note;
        std::uint32_t epoch;
    };
    std::atomic<std::uint32_t> epoch_{0};
    StaticQueue_t control_{};
    std::array<std::uint8_t, CAPACITY * sizeof(Event)> storage_{};
    QueueHandle_t queue_ = nullptr;
};
} // namespace app
