#include "esp_i2c_register_bus.h"
#include "i2c_bus.h"
#include "pca9554_controller.h"

#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <string_view>

#include <esp_io_expander_tca9554.h>

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                                       \
            std::exit(EXIT_FAILURE);                                                                                   \
        }                                                                                                              \
    } while (false)

struct i2c_master_bus_t
{
};
struct i2c_master_dev_t
{
};
struct esp_io_expander_s
{
};

namespace
{
bool bus_ready = false;
i2c_master_bus_t bus;
i2c_master_dev_t device;
int failure = 0;
int operation = 0;
int created = 0;
int deleted = 0;

esp_err_t result()
{
    return ++operation == failure ? ESP_ERR_TIMEOUT : ESP_OK;
}
} // namespace

EspI2cRegisterBus::~EspI2cRegisterBus() = default;

esp_err_t EspI2cRegisterBus::open(i2c_master_bus_handle_t handle, std::uint16_t address)
{
    CHECK(handle == &bus && address == 0x3F);
    if (failure == 6)
    {
        return ESP_ERR_TIMEOUT;
    }
    device_ = &device;
    return ESP_OK;
}

std::int32_t EspI2cRegisterBus::write(std::uint8_t reg, std::span<const std::uint8_t> data)
{
    CHECK(reg == 0x02 && data.size() == 1 && data[0] == 0);
    return failure == 7 ? ESP_ERR_TIMEOUT : ESP_OK;
}

std::int32_t EspI2cRegisterBus::read(std::uint8_t reg, std::span<std::uint8_t> data)
{
    CHECK(reg == 0 && data.size() == 1);
    data[0] = drivers::Pca9554::BUTTON_UP;
    return ESP_OK;
}

namespace drivers
{
esp_err_t I2cBus::init()
{
    bus_ready = true;
    return ESP_OK;
}
bool I2cBus::is_initialized()
{
    return bus_ready;
}
i2c_master_bus_handle_t I2cBus::handle()
{
    return &bus;
}
esp_err_t I2cBus::open(EspI2cRegisterBus& registers, std::uint16_t address)
{
    return registers.open(&bus, address);
}
} // namespace drivers

esp_err_t esp_io_expander_new_i2c_tca9554(i2c_master_bus_handle_t handle, std::uint32_t address,
                                          esp_io_expander_handle_t* output)
{
    CHECK(handle == &bus && address == 0x3F);
    *output = new esp_io_expander_s;
    ++created;
    return result();
}

esp_err_t esp_io_expander_set_dir(esp_io_expander_handle_t handle, std::uint32_t pins, esp_io_expander_dir_t direction)
{
    CHECK(handle != nullptr);
    if (operation == 1)
    {
        CHECK(pins == 0xFF && direction == IO_EXPANDER_OUTPUT);
    }
    else
    {
        CHECK(operation == 3);
        CHECK(pins == (drivers::Pca9554::BUTTON_UP | drivers::Pca9554::BUTTON_DOWN));
        CHECK(direction == IO_EXPANDER_INPUT);
    }
    return result();
}

esp_err_t esp_io_expander_set_level(esp_io_expander_handle_t handle, std::uint32_t pins, std::uint32_t level)
{
    CHECK(handle != nullptr && operation == 2 && pins == 0xFF && level == 0);
    return result();
}

esp_err_t esp_io_expander_print_state(esp_io_expander_handle_t handle)
{
    CHECK(handle != nullptr && operation == 4);
    return result();
}

esp_err_t esp_io_expander_del(esp_io_expander_handle_t handle)
{
    CHECK(handle != nullptr && deleted == 0);
    delete handle;
    ++deleted;
    return ESP_OK;
}

int main(int argc, char** argv)
{
    CHECK(argc == 2);
    const std::string_view argument{argv[1]};
    const auto parsed = std::from_chars(argument.data(), argument.data() + argument.size(), failure);
    CHECK(parsed.ec == std::errc{} && parsed.ptr == argument.data() + argument.size());
    CHECK(failure >= 0 && failure <= 7);
    CHECK(drivers::Pca9554::init() == ESP_ERR_INVALID_STATE);
    CHECK(!drivers::Pca9554::get_level(drivers::Pca9554::BUTTON_UP));
    CHECK(drivers::I2cBus::init() == ESP_OK);
    const auto err = drivers::Pca9554::init();
    if (failure != 0)
    {
        CHECK(err == ESP_ERR_TIMEOUT);
        CHECK(!drivers::Pca9554::is_initialized());
        CHECK(drivers::Pca9554::handle() == nullptr);
        CHECK(created == (failure <= 5 ? 1 : 0));
        CHECK(deleted == created);
    }
    else
    {
        CHECK(err == ESP_OK && operation == 5);
        CHECK(drivers::Pca9554::is_initialized());
        CHECK(created == 1 && deleted == 0);
        CHECK(drivers::Pca9554::init() == ESP_ERR_INVALID_STATE);
        CHECK(drivers::Pca9554::get_level(drivers::Pca9554::BUTTON_UP) == true);
        CHECK(drivers::Pca9554::get_level(drivers::Pca9554::BUTTON_DOWN) == false);
        CHECK(esp_io_expander_del(drivers::Pca9554::handle()) == ESP_OK);
        CHECK(deleted == 1);
    }
    return EXIT_SUCCESS;
}
