#pragma once
// Minimal host stand-in for ESP-IDF's esp_check.h: same control flow, logging discarded.

#include "esp_err.h"
#include "esp_log.h"

#define ESP_RETURN_ON_FALSE(condition, err_code, tag, ...)                                                             \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            static_cast<void>(tag);                                                                                    \
            return err_code;                                                                                           \
        }                                                                                                              \
    } while (false)

#define ESP_RETURN_ON_ERROR(expression, tag, ...)                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        const esp_err_t error = (expression);                                                                          \
        if (error != ESP_OK)                                                                                           \
        {                                                                                                              \
            ESP_LOGE(tag, __VA_ARGS__);                                                                                \
            return error;                                                                                              \
        }                                                                                                              \
    } while (false)
