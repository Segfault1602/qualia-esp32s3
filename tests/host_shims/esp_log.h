#pragma once
// Minimal host stand-in for ESP-IDF's esp_log.h: logging is discarded in native tests.

#define ESP_LOGE(tag, ...) static_cast<void>(tag)
#define ESP_LOGW(tag, ...) static_cast<void>(tag)
#define ESP_LOGI(tag, ...) static_cast<void>(tag)
