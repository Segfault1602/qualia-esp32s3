#pragma once

#define I2C_MASTER_PORT (0)
#define I2C_MASTER_SCL_IO (18)        /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO (8)         /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_FREQ_HZ (400000)   /*!< I2C master clock frequency */
#define I2C_MASTER_TIMEOUT_MS (1000)  /*!< I2C timeout in milliseconds */
#define I2C_MASTER_TX_BUF_DISABLE (0) /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE (0) /*!< I2C master doesn't need buffer */

#define PCA9554_ADDR (0x3F)
#define PCA9554_INPUT_PORT_REG (0x00)
#define PCA9554_OUTPUT_PORT_REG (0x01)
#define PCA9554_INVERSION_PORT_REG (0x02)
#define PCA9554_CONFIG_PORT_REG (0x03)

#if 0
#define PCA9554_TFT_BACKLIGHT (4)
#define PCA9554_BUTTON_UP (5)
#define PCA9554_BUTTON_DOWN (6)
#else

#endif

// Task Priority
#define BUTTON_TASK_PRIORITY (2)