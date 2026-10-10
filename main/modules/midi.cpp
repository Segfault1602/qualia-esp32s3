#include "modules/midi.h"

#include <chrono>

#include <esp_log.h>

#include "midi_usb.h"

namespace
{
constexpr const char* TAG = "MIDI";
} // namespace

namespace modules
{
esp_err_t Midi::init()
{
    const esp_err_t err = drivers::MidiUsb::init();
    initialized_ = err == ESP_OK;
    return err;
}

bool Midi::is_initialized() const
{
    return initialized_;
}

void Midi::step(std::chrono::microseconds now)
{
    const long long now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    for (auto packet = drivers::MidiUsb::read_packet(); packet; packet = drivers::MidiUsb::read_packet())
    {
        const auto& data = *packet;
        ESP_LOGI(TAG, "Read - Time (ms since boot): %lld, Data: %02hhX %02hhX %02hhX %02hhX", now_ms, data[0], data[1],
                 data[2], data[3]);
    }
}
} // namespace modules
