#pragma once

#include <cstdint>

#include <esp_err.h>
#include <tusb.h>

enum tinyusb_event_id_t
{
    TINYUSB_EVENT_ATTACHED,
    TINYUSB_EVENT_DETACHED,
    TINYUSB_EVENT_SUSPENDED,
    TINYUSB_EVENT_RESUMED,
};
struct tinyusb_event_t
{
    tinyusb_event_id_t id;
};
struct tinyusb_desc_config_t
{
    const tusb_desc_device_t* device = nullptr;
    const char** string = nullptr;
    int string_count = 0;
    const std::uint8_t* full_speed_config = nullptr;
};
struct tinyusb_config_t
{
    tinyusb_desc_config_t descriptor;
    void (*event_cb)(tinyusb_event_t*, void*) = nullptr;
};
esp_err_t tinyusb_driver_install(const tinyusb_config_t* config);
