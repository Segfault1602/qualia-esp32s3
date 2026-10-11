#include "midi_usb.h"

#include <array>
#include <atomic>
#include <cstdio>

#include <device/dcd.h>
#include <esp_check.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <midi2_device.h>
#include <tinyusb.h>
#include <tinyusb_default_config.h>

#include "midi_endpoint.h"

namespace
{
constexpr const char* TAG = "MIDI";
constexpr UBaseType_t RX_CAPACITY = 32;
static_assert(CFG_TUD_MIDI2_RX_BUFSIZE >= 64 + 16);
static_assert(CFG_TUD_MIDI2_RX_BUFSIZE <= 16 * 4 * sizeof(std::uint32_t));
constexpr auto CONFIG_DESCRIPTOR = std::to_array<std::uint8_t>({
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, TUD_CONFIG_DESC_LEN + TUD_MIDI2_DESC_LEN, 0, 100),
    TUD_MIDI2_DESCRIPTOR(0, 4, 1, 0x81, 64),
});
static_assert(CONFIG_DESCRIPTOR.size() == TUD_CONFIG_DESC_LEN + TUD_MIDI2_DESC_LEN);

constexpr tusb_desc_device_t DEVICE_DESCRIPTOR{
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0,
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x303A,
    .idProduct = 0x4062,
    .bcdDevice = 0x0100,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

std::array<char, 13> g_serial{};
std::array<const char*, 5> g_strings{"\x09\x04", "Qualia demo", "Qualia MIDI2", g_serial.data(), "Bounce"};

struct Received
{
    midi::Message message;
    std::uint32_t generation;
};
StaticQueue_t g_rx_control{};
std::array<std::uint8_t, RX_CAPACITY * sizeof(Received)> g_rx_storage{};
QueueHandle_t g_rx = nullptr;
std::atomic<std::uint32_t> g_state{0};
std::atomic<bool> g_available{false};
std::atomic<bool> g_overflow{false};
bool g_initialized = false;
midi::Endpoint g_endpoint;
bool g_replied = false;

enum class TxStage
{
    Idle,
    Pending,
    Sending,
    Done,
};
struct Transmission
{
    midi::Message message;
    midi::State state;
    std::atomic<esp_err_t> result{ESP_ERR_NOT_FINISHED};
    std::atomic<TxStage> stage{TxStage::Idle};
};
Transmission g_tx;

void publish(bool online, midi::Format format, bool invalidate = false)
{
    auto previous = g_state.load();
    for (;;)
    {
        const auto flags =
            static_cast<std::uint32_t>(online && g_available.load()) | (static_cast<std::uint32_t>(format) << 1);
        if (!invalidate && (previous & 7) == flags)
        {
            return;
        }
        const auto next = ((previous + 8) & ~7U) | flags;
        if (g_state.compare_exchange_weak(previous, next))
        {
            return;
        }
    }
}

void update_state(bool invalidate = false)
{
    const bool online = tud_midi2_mounted();
    const auto format = tud_midi2_alt_setting() == 0 ? midi::Format::UsbMidi1 : g_endpoint.protocol();
    publish(online, format, invalidate);
}

void receive_stream(const midi::Message& message)
{
    const auto previous = g_endpoint.protocol();
    g_endpoint.receive(message);
    if (previous != g_endpoint.protocol())
    {
        update_state(true);
        xQueueReset(g_rx);
    }
}

void receive(const midi::Message& message)
{
    if (message.ump && message.words[0] >> 28 == 0xF)
    {
        receive_stream(message);
        return;
    }
    const Received received{message, drivers::MidiUsb::state().generation};
    if (xQueueSend(g_rx, &received, 0) != pdPASS)
    {
        g_overflow.store(true);
        update_state(true);
        xQueueReset(g_rx);
    }
}

void usb_event(tinyusb_event_t* event, void*)
{
    if (event->id == TINYUSB_EVENT_ATTACHED)
    {
        g_available.store(true);
        g_endpoint.reset();
        update_state(true);
        tud_sof_cb_enable(true);
    }
    else if (event->id == TINYUSB_EVENT_DETACHED)
    {
        g_available.store(false);
        publish(false, midi::Format::UsbMidi1, true);
        xQueueReset(g_rx);
    }
    else if (event->id == TINYUSB_EVENT_SUSPENDED)
    {
        g_available.store(false);
        publish(false, midi::Format::UsbMidi1, true);
        xQueueReset(g_rx);
    }
    else if (event->id == TINYUSB_EVENT_RESUMED)
    {
        g_available.store(true);
        update_state(true);
        tud_sof_cb_enable(true);
    }
}
} // namespace

extern "C" void tud_event_hook_cb(std::uint8_t, std::uint32_t event, bool)
{
    if (event == DCD_EVENT_BUS_RESET || event == DCD_EVENT_UNPLUGGED)
    {
        g_available.store(false);
        publish(false, midi::Format::UsbMidi1, true);
    }
}

extern "C" void tud_midi2_set_itf_cb(std::uint8_t, std::uint8_t)
{
    g_endpoint.reset();
    update_state(true);
    xQueueReset(g_rx);
}

extern "C" tud_midi2_stream_result_t tud_midi2_stream_msg_cb(std::uint8_t, const std::uint32_t* words)
{
    const midi::Message message{{words[0], words[1], words[2], words[3]}, 4, true};
    receive_stream(message);
    return MIDI2_STREAM_HANDLED;
}

extern "C" void tud_midi2_rx_cb(std::uint8_t)
{
    for (unsigned batch = 0; batch < 16; ++batch)
    {
        if (tud_midi2_alt_setting() == 0)
        {
            midi::UsbPacket packet{};
            if (tud_midi2_packet_read(packet.data(), 1) == 0)
            {
                break;
            }
            receive(midi::Message::legacy(packet));
        }
        else
        {
            std::array<std::uint32_t, 4> words{};
            const auto count = tud_midi2_ump_read(words.data(), words.size());
            if (count == 0)
            {
                break;
            }
            for (std::uint32_t offset = 0; offset < count;)
            {
                midi::Message message{{}, midi::ump_word_count(words[offset]), true};
                for (std::uint32_t i = 0; i < message.size; ++i)
                {
                    message.words[i] = words[offset + i];
                }
                offset += message.size;
                receive(message);
            }
        }
    }
}

extern "C" void tud_sof_cb(std::uint32_t)
{
    update_state();
    const auto current = drivers::MidiUsb::state();
    if (current.connected && current.format != midi::Format::UsbMidi1)
    {
        const auto reply = g_endpoint.next(g_serial.data());
        const bool configuration = reply && ((reply->words[0] >> 16) & 0x3FF) == 6;
        const bool defer = !configuration && g_replied && g_tx.stage.load() == TxStage::Pending;
        g_replied = false;
        if (reply && !defer)
        {
            if (tud_midi2_ump_write(reply->words.data(), reply->size) == reply->size)
            {
                g_endpoint.sent();
                g_replied = true;
            }
            else if (configuration)
            {
                return;
            }
        }
    }
    else
    {
        g_replied = false;
    }
    auto pending = TxStage::Pending;
    if (!g_tx.stage.compare_exchange_strong(pending, TxStage::Sending, std::memory_order_acquire))
    {
        return;
    }
    if (g_tx.state != current)
    {
        g_tx.result.store(ESP_ERR_INVALID_STATE);
        g_tx.stage.store(TxStage::Done, std::memory_order_release);
        return;
    }
    const auto& message = g_tx.message;
    std::uint32_t written;
    if (message.ump)
    {
        written = tud_midi2_ump_write(message.words.data(), message.size);
    }
    else
    {
        const auto packet = message.packet();
        written = tud_midi2_packet_write(packet.data(), 1);
    }
    if (written == message.size)
    {
        g_tx.result.store(ESP_OK);
        g_tx.stage.store(TxStage::Done, std::memory_order_release);
    }
    else
    {
        g_tx.result.store(ESP_ERR_TIMEOUT);
        g_tx.stage.store(TxStage::Pending, std::memory_order_release);
    }
}

namespace drivers
{
esp_err_t MidiUsb::init()
{
    ESP_RETURN_ON_FALSE(!g_initialized, ESP_ERR_INVALID_STATE, TAG, "USB MIDI is already initialized");
    std::array<std::uint8_t, 6> mac{};
    ESP_RETURN_ON_ERROR(esp_read_mac(mac.data(), ESP_MAC_BASE), TAG, "Cannot read USB identity");
    std::snprintf(g_serial.data(), g_serial.size(), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4],
                  mac[5]);
    g_rx = xQueueCreateStatic(RX_CAPACITY, sizeof(Received), g_rx_storage.data(), &g_rx_control);
    ESP_RETURN_ON_FALSE(g_rx != nullptr, ESP_ERR_NO_MEM, TAG, "Cannot create MIDI input queue");
    tinyusb_config_t config = TINYUSB_DEFAULT_CONFIG(nullptr);
    config.descriptor.device = &DEVICE_DESCRIPTOR;
    config.descriptor.full_speed_config = CONFIG_DESCRIPTOR.data();
    config.descriptor.string = g_strings.data();
    config.descriptor.string_count = g_strings.size();
    config.event_cb = usb_event;
    ESP_RETURN_ON_ERROR(tinyusb_driver_install(&config), TAG, "TinyUSB initialization failed");
    g_initialized = true;
    ESP_LOGI(TAG, "USB initialization DONE");
    return ESP_OK;
}

bool MidiUsb::is_initialized()
{
    return g_initialized;
}

bool MidiUsb::connected()
{
    return g_initialized && state().connected;
}

midi::State MidiUsb::state()
{
    const auto value = g_state.load();
    return {static_cast<bool>(value & 1), static_cast<midi::Format>((value >> 1) & 3), value >> 3};
}

std::expected<std::optional<midi::Message>, esp_err_t> MidiUsb::read_message()
{
    if (!g_initialized)
    {
        return std::unexpected(ESP_ERR_INVALID_STATE);
    }
    if (g_overflow.exchange(false))
    {
        return std::unexpected(ESP_ERR_NO_MEM);
    }
    Received received{};
    for (unsigned i = 0; i < RX_CAPACITY && xQueueReceive(g_rx, &received, 0) == pdPASS; ++i)
    {
        if (received.generation == state().generation)
        {
            return received.message;
        }
    }
    return std::nullopt;
}

std::expected<void, esp_err_t> MidiUsb::write_message(const midi::Message& message, midi::State expected_state)
{
    if (!message.valid() || message.ump != (expected_state.format != midi::Format::UsbMidi1))
    {
        return std::unexpected(ESP_ERR_INVALID_ARG);
    }
    if (!expected_state.connected || expected_state != state())
    {
        return std::unexpected(ESP_ERR_INVALID_STATE);
    }
    const auto stage = g_tx.stage.load(std::memory_order_acquire);
    if (stage == TxStage::Done)
    {
        const bool matches = message == g_tx.message && expected_state == g_tx.state;
        const auto result = g_tx.result.load();
        g_tx.stage.store(TxStage::Idle, std::memory_order_release);
        if (matches)
        {
            return result == ESP_OK ? std::expected<void, esp_err_t>{} : std::unexpected(result);
        }
    }
    else if (stage == TxStage::Pending)
    {
        if (message == g_tx.message && expected_state == g_tx.state)
        {
            const auto result = g_tx.result.load();
            return std::unexpected(result == ESP_ERR_TIMEOUT ? ESP_ERR_TIMEOUT : ESP_ERR_NOT_FINISHED);
        }
        auto pending = TxStage::Pending;
        if (!g_tx.stage.compare_exchange_strong(pending, TxStage::Idle))
        {
            return std::unexpected(ESP_ERR_NOT_FINISHED);
        }
    }
    else if (stage == TxStage::Sending)
    {
        return std::unexpected(ESP_ERR_NOT_FINISHED);
    }
    g_tx.message = message;
    g_tx.state = expected_state;
    g_tx.result.store(ESP_ERR_NOT_FINISHED);
    g_tx.stage.store(TxStage::Pending, std::memory_order_release);
    return std::unexpected(ESP_ERR_NOT_FINISHED);
}
} // namespace drivers
