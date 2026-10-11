#include "midi_usb.h"

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

namespace
{
constexpr const char* TAG = "MIDI";
} // namespace

namespace drivers
{
esp_err_t MidiUsb::enter_bootloader()
{
    ESP_RETURN_ON_FALSE(is_initialized(), ESP_ERR_INVALID_STATE, TAG, "USB MIDI is not initialized");
    ESP_RETURN_ON_FALSE(!esp_efuse_read_field_bit(ESP_EFUSE_DIS_FORCE_DOWNLOAD) &&
                            !esp_efuse_read_field_bit(ESP_EFUSE_DIS_DOWNLOAD_MODE),
                        ESP_ERR_NOT_SUPPORTED, TAG, "ROM download entry is disabled by eFuse");
    ESP_LOGI(TAG, "Rebooting into ROM download mode");
    ESP_RETURN_ON_ERROR(tinyusb_driver_uninstall(), TAG, "Failed to disconnect USB MIDI");
    vTaskDelay(pdMS_TO_TICKS(200));
    chip_usb_set_persist_flags(0);
    REG_CLR_BIT(RTC_CNTL_USB_CONF_REG, RTC_CNTL_SW_HW_USB_PHY_SEL);
    REG_SET_BIT(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
    esp_rom_software_reset_system();
    __builtin_unreachable();
}
} // namespace drivers
