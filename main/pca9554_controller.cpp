#include "pca9554_controller.h"

#include "freertos/FreeRTOS.h"
#include <driver/i2c.h>
#include <esp_check.h>
#include <esp_io_expander_tca9554.h>
#include <string.h>

#include "defines.h"
#include "init_operations.h"

#define PCATAG "PCA9554"

esp_io_expander_handle_t g_io_expender = NULL;
static bool g_pca9554_initialized = false;

static esp_err_t write_register(uint8_t reg_addr, uint8_t* data, size_t size)
{
    uint8_t buf[32] = {0};
    buf[0] = reg_addr;
    memcpy(buf + 1, data, size);
    ESP_RETURN_ON_ERROR(i2c_master_write_to_device(I2C_MASTER_PORT, PCA9554_ADDR, buf, size + 1,
                                                   I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS),
                        PCATAG, "Failed to write register");
    return ESP_OK;
}

esp_err_t pca9554_init(void)
{
    i2c_port_t i2c_master_port = I2C_MASTER_PORT;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {.clk_speed = I2C_MASTER_FREQ_HZ},
    };

    i2c_param_config(i2c_master_port, &conf);

    ESP_RETURN_ON_ERROR(
        i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0), PCATAG,
        "Failed to install I2C driver");
    esp_io_expander_new_i2c_tca9554(I2C_MASTER_PORT, PCA9554_ADDR, &g_io_expender);
    // No invert
    uint8_t write_buf[2] = {PCA9554_INVERSION_PORT_REG, 0x0};
    ESP_RETURN_ON_ERROR(i2c_master_write_to_device(I2C_MASTER_PORT, PCA9554_ADDR, write_buf, sizeof(write_buf),
                                                   I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS),
                        PCATAG, "Failed to write to device");

    // All input
    write_buf[0] = PCA9554_CONFIG_PORT_REG;
    write_buf[1] = 0xFF;
    ESP_RETURN_ON_ERROR(i2c_master_write_to_device(I2C_MASTER_PORT, PCA9554_ADDR, write_buf, sizeof(write_buf),
                                                   I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS),
                        PCATAG, "Failed to write to device");

    // All low
    write_buf[0] = PCA9554_OUTPUT_PORT_REG;
    write_buf[1] = 0x0;
    ESP_RETURN_ON_ERROR(i2c_master_write_to_device(I2C_MASTER_PORT, PCA9554_ADDR, write_buf, sizeof(write_buf),
                                                   I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS),
                        PCATAG, "Failed to write to device");

    esp_io_expander_set_dir(g_io_expender, 0xFF, IO_EXPANDER_OUTPUT);
    esp_io_expander_set_level(g_io_expender, 0xFF, 0);

    esp_io_expander_set_dir(g_io_expender, PCA9554_TFT_RESET, IO_EXPANDER_OUTPUT);
    esp_io_expander_set_level(g_io_expender, PCA9554_TFT_RESET, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    esp_io_expander_set_level(g_io_expender, PCA9554_TFT_RESET, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    esp_io_expander_set_dir(g_io_expender, PCA9554_TFT_CS, IO_EXPANDER_OUTPUT);
    esp_io_expander_set_level(g_io_expender, PCA9554_TFT_CS, 1);

    esp_io_expander_set_dir(g_io_expender, PCA9554_TFT_SCK, IO_EXPANDER_OUTPUT);
    esp_io_expander_set_level(g_io_expender, PCA9554_TFT_SCK, 1);

    esp_io_expander_set_dir(g_io_expender, PCA9554_TFT_MOSI, IO_EXPANDER_OUTPUT);
    esp_io_expander_set_level(g_io_expender, PCA9554_TFT_MOSI, 1);

    esp_io_expander_print_state(g_io_expender);

    g_pca9554_initialized = true;
    return ESP_OK;
}

bool pca9554_is_init(void)
{
    return g_pca9554_initialized;
}

esp_err_t pca9554_set_pin_mode(uint8_t pin, uint8_t output)
{

    ESP_RETURN_ON_ERROR(esp_io_expander_set_dir(g_io_expender, pin, output), PCATAG, "Failed to set pin mode");

    return ESP_OK;
}

esp_err_t pca9554_digital_write(uint8_t pin, uint8_t value)
{
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(g_io_expender, pin, value), PCATAG, "Failed to write to pin");

    return ESP_OK;
}

esp_err_t pca9554_enable_backlight(bool enabled)
{
    ESP_RETURN_ON_ERROR(esp_io_expander_set_dir(g_io_expender, PCA9554_TFT_BACKLIGHT, IO_EXPANDER_OUTPUT), PCATAG,
                        "Failed to set pin mode for backlight");
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(g_io_expender, PCA9554_TFT_BACKLIGHT, enabled), PCATAG,
                        "Failed to write to backlight pin");
    return ESP_OK;
}

esp_err_t pca9554_get_pin_state(uint8_t pin, uint8_t* value)
{
    uint32_t value_mask = 0;
    ESP_RETURN_ON_ERROR(esp_io_expander_get_level(g_io_expender, pin, &value_mask), PCATAG, "Failed to read pin state");
    *value = value_mask > 0 ? 1 : 0;

    return ESP_OK;
}

static uint8_t output_buf = 0;

void digitalWrite(uint8_t pin, uint8_t val)
{
    uint8_t reg_data = output_buf;

    reg_data &= ~(1UL << pin);
    if (val == 1)
    {
        reg_data |= (1UL << pin);
    }

    write_register(PCA9554_OUTPUT_PORT_REG, &reg_data, 1);
    output_buf = reg_data;
}

void beginWrite()
{
    pca9554_digital_write(PCA9554_TFT_CS, 0);
}

void endWrite()
{
    pca9554_digital_write(PCA9554_TFT_CS, 1);
}

void writeCommand(uint8_t c)
{
    ESP_LOGI(PCATAG, "Writing command: 0x%02x", c);
    bool last_databit = 0;
    // D/C bit, command
    pca9554_digital_write(PCA9554_TFT_MOSI, last_databit);
    pca9554_digital_write(PCA9554_TFT_SCK, 0);
    pca9554_digital_write(PCA9554_TFT_SCK, 1);

    uint8_t bit = 0x80;
    while (bit)
    {
        if (c & bit)
        {
            if (last_databit != 1)
            {
                last_databit = 1;
                pca9554_digital_write(PCA9554_TFT_MOSI, last_databit);
            }
        }
        else
        {
            if (last_databit != 0)
            {
                last_databit = 0;
                pca9554_digital_write(PCA9554_TFT_MOSI, last_databit);
            }
        }
        pca9554_digital_write(PCA9554_TFT_SCK, 0);
        bit >>= 1;
        pca9554_digital_write(PCA9554_TFT_SCK, 1);
    }
}

void write(uint8_t d)
{
    bool last_databit = 1;
    pca9554_digital_write(PCA9554_TFT_MOSI, last_databit);
    pca9554_digital_write(PCA9554_TFT_SCK, 0);
    pca9554_digital_write(PCA9554_TFT_SCK, 1);

    uint8_t bit = 0x80;
    while (bit)
    {
        if (d & bit)
        {
            if (last_databit != 1)
            {
                last_databit = 1;
                // this->digitalWrite(_mosi, last_databit);
                pca9554_digital_write(PCA9554_TFT_MOSI, last_databit);
            }
        }
        else
        {
            if (last_databit != 0)
            {
                last_databit = 0;
                // this->digitalWrite(_mosi, last_databit);
                pca9554_digital_write(PCA9554_TFT_MOSI, last_databit);
            }
        }
        // this->digitalWrite(_sck, 0);
        pca9554_digital_write(PCA9554_TFT_SCK, 0);
        bit >>= 1;
        // this->digitalWrite(_sck, 1);
        pca9554_digital_write(PCA9554_TFT_SCK, 1);
    }
}

void writeCommand16(uint16_t data)
{
    assert(false);
}

void writeCommandBytes(uint8_t* data, uint32_t len)
{
    assert(false);
}

union {
    uint16_t value;
    struct
    {
        uint8_t lsb;
        uint8_t msb;
    };
} _data16;

esp_err_t batch_operation(const uint8_t* operations, size_t len)
{
    for (size_t i = 0; i < len; ++i)
    {
        uint8_t l = 0;
        switch (operations[i])
        {
        case BEGIN_WRITE:
            beginWrite();
            break;
        case WRITE_C8_D16:
            l++;
            /* fall through */
        case WRITE_C8_D8:
            l++;
            /* fall through */
        case WRITE_COMMAND_8:
            writeCommand(operations[++i]);
            break;
        case WRITE_C16_D16:
            l = 2;
            /* fall through */
        case WRITE_COMMAND_16:
            _data16.msb = operations[++i];
            _data16.lsb = operations[++i];
            writeCommand16(_data16.value);
            break;
        case WRITE_COMMAND_BYTES:
            l = operations[++i];
            writeCommandBytes((uint8_t*)(operations + i + 1), l);
            i += l;
            l = 0;
            break;
        case WRITE_DATA_8:
            l = 1;
            break;
        case WRITE_DATA_16:
            l = 2;
            break;
        case WRITE_BYTES:
            l = operations[++i];
            break;
        case WRITE_C8_BYTES:
            writeCommand(operations[++i]);
            l = operations[++i];
            break;
        case END_WRITE:
            endWrite();
            break;
        case DELAY:
            vTaskDelay(pdMS_TO_TICKS(operations[++i]));
            break;
        default:
            printf("Unknown operation id at %d: %d\n", i, operations[i]);
            break;
        }
        while (l--)
        {
            write(operations[++i]);
        }
    }

    return ESP_OK;
}