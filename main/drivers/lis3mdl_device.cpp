#include "lis3mdl_device.h"

#include <lis3mdl_reg.h>

namespace imu
{
namespace
{
constexpr int RESET_POLL_LIMIT = 100;
constexpr float MICROTESLA_PER_GAUSS = 100.0f;
// Over I2C the LIS3MDL only advances the register address when bit 7 of the sub-address is set.
constexpr std::uint8_t AUTO_INCREMENT = 0x80;

std::uint8_t sub_address(std::uint8_t reg, std::uint16_t length)
{
    return length > 1 ? static_cast<std::uint8_t>(reg | AUTO_INCREMENT) : reg;
}

std::int32_t write_reg(void* handle, std::uint8_t reg, const std::uint8_t* data, std::uint16_t length)
{
    return static_cast<RegisterBus*>(handle)->write(sub_address(reg, length), {data, length});
}

std::int32_t read_reg(void* handle, std::uint8_t reg, std::uint8_t* data, std::uint16_t length)
{
    return static_cast<RegisterBus*>(handle)->read(sub_address(reg, length), {data, length});
}

stmdev_ctx_t make_context(RegisterBus& bus)
{
    stmdev_ctx_t context{};
    context.write_reg = write_reg;
    context.read_reg = read_reg;
    context.handle = &bus;
    return context;
}
} // namespace

Lis3mdlDevice::Lis3mdlDevice(RegisterBus& bus) : bus_(bus)
{
}

std::expected<void, Error> Lis3mdlDevice::init()
{
    const stmdev_ctx_t context = make_context(bus_);

    std::uint8_t id = 0;
    if (lis3mdl_device_id_get(&context, &id) != 0)
    {
        return std::unexpected(Error::Bus);
    }
    if (id != DEVICE_ID)
    {
        return std::unexpected(Error::WrongDevice);
    }

    if (lis3mdl_reset_set(&context, PROPERTY_ENABLE) != 0)
    {
        return std::unexpected(Error::Bus);
    }
    std::uint8_t resetting = 1;
    for (int attempt = 0; resetting != 0; ++attempt)
    {
        if (attempt == RESET_POLL_LIMIT)
        {
            return std::unexpected(Error::Timeout);
        }
        if (lis3mdl_reset_get(&context, &resetting) != 0)
        {
            return std::unexpected(Error::Bus);
        }
    }

    std::int32_t status = lis3mdl_block_data_update_set(&context, PROPERTY_ENABLE);
    status |= lis3mdl_full_scale_set(&context, LIS3MDL_4_GAUSS);
    status |= lis3mdl_data_rate_set(&context, LIS3MDL_UHP_80Hz);
    status |= lis3mdl_operating_mode_set(&context, LIS3MDL_CONTINUOUS_MODE);
    if (status != 0)
    {
        return std::unexpected(Error::Bus);
    }
    return {};
}

std::expected<Vec3, Error> Lis3mdlDevice::read()
{
    const stmdev_ctx_t context = make_context(bus_);

    std::int16_t raw[3] = {};
    if (lis3mdl_magnetic_raw_get(&context, raw) != 0)
    {
        return std::unexpected(Error::Bus);
    }
    return Vec3{
        lis3mdl_from_fs4_to_gauss(raw[0]) * MICROTESLA_PER_GAUSS,
        lis3mdl_from_fs4_to_gauss(raw[1]) * MICROTESLA_PER_GAUSS,
        lis3mdl_from_fs4_to_gauss(raw[2]) * MICROTESLA_PER_GAUSS,
    };
}
} // namespace imu
