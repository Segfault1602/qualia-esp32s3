#include "lsm6dsox_device.h"

#include <array>
#include <numbers>
#include <span>

#include <lsm6dsox_reg.h>

namespace imu
{
namespace
{
constexpr int RESET_POLL_LIMIT = 100;
constexpr float STANDARD_GRAVITY = 9.80665f;

std::int32_t write_reg(void* handle, std::uint8_t reg, const std::uint8_t* data, std::uint16_t length)
{
    return static_cast<RegisterBus*>(handle)->write(reg, {data, length});
}

std::int32_t read_reg(void* handle, std::uint8_t reg, std::uint8_t* data, std::uint16_t length)
{
    return static_cast<RegisterBus*>(handle)->read(reg, {data, length});
}

stmdev_ctx_t make_context(RegisterBus& bus)
{
    stmdev_ctx_t context{};
    context.write_reg = write_reg;
    context.read_reg = read_reg;
    context.handle = &bus;
    return context;
}

std::int16_t little_endian(std::span<const std::uint8_t, 2> bytes)
{
    return static_cast<std::int16_t>(static_cast<std::uint16_t>(bytes[0]) | static_cast<std::uint16_t>(bytes[1]) << 8);
}

float mg_to_mps2(float mg)
{
    return mg * STANDARD_GRAVITY / 1000.0f;
}

float mdps_to_radps(float mdps)
{
    return mdps * std::numbers::pi_v<float> / 180.0f / 1000.0f;
}
} // namespace

Lsm6dsoxDevice::Lsm6dsoxDevice(RegisterBus& bus) : bus_(bus)
{
}

std::expected<void, Error> Lsm6dsoxDevice::init()
{
    const stmdev_ctx_t context = make_context(bus_);

    // The sensor keeps power across MCU resets and may still have the embedded-function bank selected,
    // which hides WHO_AM_I. FUNC_CFG_ACCESS is visible from every bank.
    if (lsm6dsox_mem_bank_set(&context, LSM6DSOX_USER_BANK) != 0)
    {
        return std::unexpected(Error::Bus);
    }

    std::uint8_t id = 0;
    if (lsm6dsox_device_id_get(&context, &id) != 0)
    {
        return std::unexpected(Error::Bus);
    }
    if (id != DEVICE_ID)
    {
        return std::unexpected(Error::WrongDevice);
    }

    if (lsm6dsox_reset_set(&context, PROPERTY_ENABLE) != 0)
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
        if (lsm6dsox_reset_get(&context, &resetting) != 0)
        {
            return std::unexpected(Error::Bus);
        }
    }

    std::int32_t status = lsm6dsox_i3c_disable_set(&context, LSM6DSOX_I3C_DISABLE);
    status |= lsm6dsox_block_data_update_set(&context, PROPERTY_ENABLE);
    status |= lsm6dsox_xl_full_scale_set(&context, LSM6DSOX_2g);
    status |= lsm6dsox_gy_full_scale_set(&context, LSM6DSOX_500dps);
    status |= lsm6dsox_xl_data_rate_set(&context, LSM6DSOX_XL_ODR_104Hz);
    status |= lsm6dsox_gy_data_rate_set(&context, LSM6DSOX_GY_ODR_104Hz);
    if (status != 0)
    {
        return std::unexpected(Error::Bus);
    }
    return {};
}

std::expected<MotionSample, Error> Lsm6dsoxDevice::read()
{
    const stmdev_ctx_t context = make_context(bus_);

    // Gyroscope then accelerometer outputs are contiguous, so one burst covers both.
    constexpr std::size_t OUTPUT_BYTES = LSM6DSOX_OUTZ_H_A - LSM6DSOX_OUTX_L_G + 1;
    std::array<std::uint8_t, OUTPUT_BYTES> raw{};
    if (lsm6dsox_read_reg(&context, LSM6DSOX_OUTX_L_G, raw.data(), raw.size()) != 0)
    {
        return std::unexpected(Error::Bus);
    }

    MotionSample sample;
    const std::span bytes{raw};
    sample.gyro.x = mdps_to_radps(lsm6dsox_from_fs500_to_mdps(little_endian(bytes.subspan<0, 2>())));
    sample.gyro.y = mdps_to_radps(lsm6dsox_from_fs500_to_mdps(little_endian(bytes.subspan<2, 2>())));
    sample.gyro.z = mdps_to_radps(lsm6dsox_from_fs500_to_mdps(little_endian(bytes.subspan<4, 2>())));
    sample.accel.x = mg_to_mps2(lsm6dsox_from_fs2_to_mg(little_endian(bytes.subspan<6, 2>())));
    sample.accel.y = mg_to_mps2(lsm6dsox_from_fs2_to_mg(little_endian(bytes.subspan<8, 2>())));
    sample.accel.z = mg_to_mps2(lsm6dsox_from_fs2_to_mg(little_endian(bytes.subspan<10, 2>())));
    return sample;
}
} // namespace imu
