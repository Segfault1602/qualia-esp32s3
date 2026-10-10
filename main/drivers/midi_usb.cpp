#include "midi_usb.h"

#include <array>

#include <esp32s3/rom/usb/chip_usb_dw_wrapper.h>
#include <esp_check.h>
#include <esp_efuse.h>
#include <esp_efuse_table.h>
#include <esp_log.h>
#include <esp_rom_sys.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <soc/rtc_cntl_reg.h>
#include <soc/soc.h>
#include <tinyusb.h>
#include <tinyusb_default_config.h>
#include <tusb.h>

namespace
{
constexpr const char* TAG = "MIDI";

constexpr std::uint8_t MIDI_INTERFACE = 0;
constexpr std::uint8_t INTERFACE_COUNT = CFG_TUD_MIDI ? 2 : 0;
constexpr std::uint8_t MIDI_ENDPOINT = 1;
constexpr int TUSB_DESCRIPTOR_TOTAL_LEN = TUD_CONFIG_DESC_LEN + CFG_TUD_MIDI * TUD_MIDI_DESC_LEN;

std::array<const char*, 5> STRING_DESCRIPTORS{
    "\x09\x04", "TinyUSB", "TinyUSB Device", "123456", "Example MIDI device",
};

constexpr auto MIDI_CONFIG_DESCRIPTOR = std::to_array<std::uint8_t>({
    // Configuration number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, INTERFACE_COUNT, 0, TUSB_DESCRIPTOR_TOTAL_LEN, 0, 100),
#if CFG_TUD_MIDI
    // Interface number, string index, EP Out & EP In address, EP size
    TUD_MIDI_DESCRIPTOR(MIDI_INTERFACE, 4, MIDI_ENDPOINT, (0x80 | MIDI_ENDPOINT), 64),
#endif
});
static_assert(MIDI_CONFIG_DESCRIPTOR.size() == TUSB_DESCRIPTOR_TOTAL_LEN);

bool g_initialized = false;
} // namespace

namespace drivers
{
esp_err_t MidiUsb::init()
{
    ESP_RETURN_ON_FALSE(!g_initialized, ESP_ERR_INVALID_STATE, TAG, "USB MIDI is already initialized");

    // Strict ISO C++ keeps the comma in the macro's `, ##__VA_ARGS__`, so the no-argument form doesn't expand.
    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG(nullptr);
    tusb_cfg.descriptor.string = STRING_DESCRIPTORS.data();
    tusb_cfg.descriptor.string_count = STRING_DESCRIPTORS.size();
    tusb_cfg.descriptor.full_speed_config = MIDI_CONFIG_DESCRIPTOR.data();
    ESP_RETURN_ON_ERROR(tinyusb_driver_install(&tusb_cfg), TAG, "TinyUSB initialization failed");

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
    return g_initialized && tud_mounted();
}

std::expected<void, esp_err_t> MidiUsb::write_packet(const Packet& packet)
{
    if (!connected())
    {
        return std::unexpected(ESP_ERR_INVALID_STATE);
    }
    if (!tud_midi_packet_write(packet.data()))
    {
        return std::unexpected(ESP_ERR_TIMEOUT);
    }
    return {};
}

esp_err_t MidiUsb::enter_bootloader()
{
    ESP_RETURN_ON_FALSE(g_initialized, ESP_ERR_INVALID_STATE, TAG, "USB MIDI is not initialized");
    ESP_RETURN_ON_FALSE(!esp_efuse_read_field_bit(ESP_EFUSE_DIS_FORCE_DOWNLOAD) &&
                            !esp_efuse_read_field_bit(ESP_EFUSE_DIS_DOWNLOAD_MODE),
                        ESP_ERR_NOT_SUPPORTED, TAG, "ROM download entry is disabled by eFuse");
    ESP_LOGI(TAG, "Rebooting into ROM download mode");
    ESP_RETURN_ON_ERROR(tinyusb_driver_uninstall(), TAG, "Failed to disconnect USB MIDI");
    g_initialized = false;
    vTaskDelay(pdMS_TO_TICKS(200));
    chip_usb_set_persist_flags(0);
    REG_CLR_BIT(RTC_CNTL_USB_CONF_REG, RTC_CNTL_SW_HW_USB_PHY_SEL);
    REG_SET_BIT(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
    esp_rom_software_reset_system();
    __builtin_unreachable();
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
