#pragma once

#include <driver/gpio.h>

#define I2C_MASTER_PORT I2C_NUM_0
#define I2C_MASTER_SCL_IO GPIO_NUM_18 /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO GPIO_NUM_8  /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_FREQ_HZ (400000)   /*!< I2C master clock frequency */
#define I2C_MASTER_TIMEOUT_MS (20)    /*!< I2C timeout in milliseconds; bounds how long one failed transfer stalls a task */

#define PCA9554_ADDR (0x3F)
#define PCA9554_INPUT_PORT_REG (0x00)
#define PCA9554_INVERSION_PORT_REG (0x02)