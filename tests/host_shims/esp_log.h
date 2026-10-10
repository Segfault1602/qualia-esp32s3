#pragma once
// Minimal host stand-in for ESP-IDF's esp_log.h: logging is discarded in native tests.

template <typename... Args>
inline void host_log(const char*, const char*, const Args&...)
{
}

#define ESP_LOGE(tag, ...) host_log(tag, __VA_ARGS__)
#define ESP_LOGW(tag, ...) host_log(tag, __VA_ARGS__)
#define ESP_LOGI(tag, ...) host_log(tag, __VA_ARGS__)
