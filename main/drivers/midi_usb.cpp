#include "midi_usb.h"

#include <iterator>

#include <esp_check.h>
#include <esp_log.h>
#include <tinyusb.h>
#include <tinyusb_default_config.h>
#include <tusb.h>

namespace
{
constexpr const char* TAG = "MIDI";

// Interface counter
enum interface_count
{
#if CFG_TUD_MIDI
    ITF_NUM_MIDI = 0,
    ITF_NUM_MIDI_STREAMING,
#endif
    ITF_COUNT
};

// USB Endpoint numbers
enum usb_endpoints
{
    // Available USB Endpoints: 5 IN/OUT EPs and 1 IN EP
    EP_EMPTY = 0,
#if CFG_TUD_MIDI
    EPNUM_MIDI,
#endif
};

/** TinyUSB descriptors **/

constexpr int TUSB_DESCRIPTOR_TOTAL_LEN = TUD_CONFIG_DESC_LEN + CFG_TUD_MIDI * TUD_MIDI_DESC_LEN;

const char* s_str_desc[] = {
    // array of pointer to string descriptors
    "\x09\x04",            // 0: is supported language is English (0x0409)
    "TinyUSB",             // 1: Manufacturer
    "TinyUSB Device",      // 2: Product
    "123456",              // 3: Serials, should use chip ID
    "Example MIDI device", // 4: MIDI
};

const uint8_t s_midi_cfg_desc[] = {
    // Configuration number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_COUNT, 0, TUSB_DESCRIPTOR_TOTAL_LEN, 0, 100),
#if CFG_TUD_MIDI
    // Interface number, string index, EP Out & EP In address, EP size
    TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI, 4, EPNUM_MIDI, (0x80 | EPNUM_MIDI), 64),
#endif
};

bool g_initialized = false;
} // namespace

namespace drivers
{
esp_err_t MidiUsb::init()
{
    ESP_RETURN_ON_FALSE(!g_initialized, ESP_ERR_INVALID_STATE, TAG, "USB MIDI is already initialized");

    // Strict ISO C++ keeps the comma in the macro's `, ##__VA_ARGS__`, so the no-argument form doesn't expand.
    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG(nullptr);
    tusb_cfg.descriptor.string = s_str_desc;
    tusb_cfg.descriptor.string_count = std::size(s_str_desc);
    tusb_cfg.descriptor.full_speed_config = s_midi_cfg_desc;
    ESP_RETURN_ON_ERROR(tinyusb_driver_install(&tusb_cfg), TAG, "TinyUSB initialization failed");

    g_initialized = true;
    ESP_LOGI(TAG, "USB initialization DONE");
    return ESP_OK;
}

bool MidiUsb::is_initialized()
{
    return g_initialized;
}

std::optional<MidiUsb::Packet> MidiUsb::read_packet()
{
    if (!g_initialized || !tud_midi_available())
    {
        return std::nullopt;
    }
    Packet packet{};
    if (!tud_midi_packet_read(packet.data()))
    {
        ESP_LOGE(TAG, "Failed to read an available MIDI packet");
        return std::nullopt;
    }
    return packet;
}
} // namespace drivers
