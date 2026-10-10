#pragma once

#include <cstdint>

#include <driver/i2c_master.h>
#include <esp_err.h>

class EspI2cRegisterBus;

namespace drivers
{
/**
 * @brief The board I2C bus shared by the I/O expander and the STEMMA QT port.
 *
 * Sharing rules:
 * - Each transfer is atomic across tasks. ESP-IDF's i2c_master driver holds one lock per bus around every transfer,
 *   for every device handle on the bus, including handles third-party drivers attach through handle().
 * - A sequence of transfers is not atomic. A device whose state spans several transfers or a software cache (the
 *   expander's output register) needs a single owner, or a lock covering the whole operation.
 * - Startup runs before any task, so init() sequences of every driver are serialized.
 *
 * Transfers through open() devices time out after I2C_MASTER_TIMEOUT_MS.
 */
class I2cBus
{
  public:
    I2cBus() = delete;

    static esp_err_t init();
    static bool is_initialized();

    /**
     * @brief Attach device to the bus at a 7-bit address.
     */
    static esp_err_t open(EspI2cRegisterBus& device, std::uint16_t address);

    /**
     * @brief Raw bus handle, or nullptr before init() succeeds.
     *
     * Only for third-party drivers that attach their own devices (the TCA9554 expander component); use open()
     * otherwise.
     */
    static i2c_master_bus_handle_t handle();
};
} // namespace drivers
