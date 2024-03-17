#pragma once

#include <esp_err.h>
#include <stdbool.h>

#define BUTTON_UP (0)
#define BUTTON_DOWN (1)
#define BUTTON_COUNT (2)

typedef void (*button_rising_edge_cb_t)(uint8_t button);

esp_err_t buttons_init();

void register_button_rising_edge_callback(button_rising_edge_cb_t cb);