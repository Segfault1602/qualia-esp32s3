#pragma once
// Minimal host stand-in for ESP-IDF's esp_err.h, so modules/ headers build in native tests.

typedef int esp_err_t;

#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_TIMEOUT 0x107
#define ESP_ERR_NOT_FOUND 0x105

inline const char* esp_err_to_name(esp_err_t)
{
    return "esp_err";
}
