#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "esp_err.h"

namespace host_trace
{
inline bool connected = false;
inline esp_err_t write_result = ESP_OK;
inline std::vector<std::string> records;
inline int writes = 0;
} // namespace host_trace

inline bool esp_apptrace_host_is_connected()
{
    return host_trace::connected;
}

inline esp_err_t esp_apptrace_write(const void* data, std::size_t size, unsigned)
{
    ++host_trace::writes;
    if (host_trace::write_result == ESP_OK)
    {
        host_trace::records.emplace_back(static_cast<const char*>(data), size);
    }
    return host_trace::write_result;
}
