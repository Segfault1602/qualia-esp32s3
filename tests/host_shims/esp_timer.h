#pragma once

#include <cstdint>

namespace host_timer
{
inline std::int64_t now = 0;
}

inline std::int64_t esp_timer_get_time()
{
    const auto time = host_timer::now;
    host_timer::now += 100;
    return time;
}
