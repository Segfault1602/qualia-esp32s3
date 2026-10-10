#pragma once

#include "driver/i2c_master.h"
#include "esp_io_expander.h"

esp_err_t esp_io_expander_new_i2c_tca9554(i2c_master_bus_handle_t bus, std::uint32_t address,
                                          esp_io_expander_handle_t* handle);
