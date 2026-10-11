#pragma once

#include <cstdint>

#include <esp_err.h>

constexpr int ESP_MAC_BASE = 0;
esp_err_t esp_read_mac(std::uint8_t* mac, int type);
