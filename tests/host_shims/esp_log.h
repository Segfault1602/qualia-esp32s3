#pragma once
// Minimal host stand-in for ESP-IDF's esp_log.h: logging is discarded in native tests.

inline void (*host_log_observer)(const char*, const char*) = nullptr;

template <typename... Args>
inline void host_log(const char* tag, const char* format, const Args&...)
{
    if (host_log_observer != nullptr)
    {
        host_log_observer(tag, format);
    }
}

#define ESP_LOGE(tag, ...) host_log(tag, __VA_ARGS__)
#define ESP_LOGW(tag, ...) host_log(tag, __VA_ARGS__)
#define ESP_LOGI(tag, ...) host_log(tag, __VA_ARGS__)
