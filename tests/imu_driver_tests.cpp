#include "lis3mdl_device.h"
#include "lsm6dsox_device.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#include <numbers>

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                                       \
            std::exit(EXIT_FAILURE);                                                                                   \
        }                                                                                                              \
    } while (false)

namespace
{
bool near(float actual, float expected, float tolerance = 1e-3f)
{
    return std::fabs(actual - expected) <= tolerance;
}

// Flat 128-register device. When msb_auto_increment is set, multi-byte transfers only advance the
// address if bit 7 of the sub-address is set, as on the LIS3MDL over I2C.
class FakeDevice final : public imu::RegisterBus
{
  public:
    std::array<std::uint8_t, 128> regs{};
    bool msb_auto_increment = false;
    bool fail = false;
    int writes = 0;
    std::function<void(FakeDevice&, std::uint8_t reg)> on_write;
    // Returns true and sets value to replace what a register reads back.
    std::function<bool(const FakeDevice&, std::uint8_t reg, std::uint8_t& value)> read_override;

    std::int32_t read(std::uint8_t reg, std::span<std::uint8_t> data) override
    {
        if (fail)
        {
            return -1;
        }
        bool increment = true;
        std::uint8_t address = reg;
        if (msb_auto_increment)
        {
            increment = (reg & 0x80) != 0;
            address = reg & 0x7F;
        }
        for (std::uint8_t& byte : data)
        {
            byte = regs.at(address);
            if (read_override)
            {
                read_override(*this, address, byte);
            }
            address += increment ? 1 : 0;
        }
        return 0;
    }

    std::int32_t write(std::uint8_t reg, std::span<const std::uint8_t> data) override
    {
        if (fail)
        {
            return -1;
        }
        bool increment = true;
        std::uint8_t address = reg;
        if (msb_auto_increment)
        {
            increment = (reg & 0x80) != 0;
            address = reg & 0x7F;
        }
        for (const std::uint8_t byte : data)
        {
            regs.at(address) = byte;
            ++writes;
            if (on_write)
            {
                on_write(*this, address);
            }
            address += increment ? 1 : 0;
        }
        return 0;
    }

    void put16(std::uint8_t reg, std::int16_t value)
    {
        const auto bits = static_cast<std::uint16_t>(value);
        regs.at(reg) = static_cast<std::uint8_t>(bits & 0xFF);
        regs.at(reg + 1) = static_cast<std::uint8_t>(bits >> 8);
    }
};

constexpr std::uint8_t WHO_AM_I = 0x0F;

namespace lsm
{
constexpr std::uint8_t FUNC_CFG_ACCESS = 0x01;
constexpr std::uint8_t CTRL1_XL = 0x10;
constexpr std::uint8_t CTRL2_G = 0x11;
constexpr std::uint8_t CTRL3_C = 0x12;
constexpr std::uint8_t CTRL9_XL = 0x18;
constexpr std::uint8_t OUTX_L_G = 0x22;
constexpr std::uint8_t OUTX_L_A = 0x28;

FakeDevice make_device()
{
    FakeDevice device;
    device.regs[WHO_AM_I] = imu::Lsm6dsoxDevice::DEVICE_ID;
    device.regs[CTRL3_C] = 0x04;
    device.regs[CTRL9_XL] = 0xE0;
    device.on_write = [](FakeDevice& self, std::uint8_t reg) {
        if (reg == CTRL3_C && (self.regs[CTRL3_C] & 0x01) != 0)
        {
            for (std::uint8_t r = CTRL1_XL; r <= 0x19; ++r)
            {
                self.regs[r] = 0;
            }
            self.regs[CTRL3_C] = 0x04;
            self.regs[CTRL9_XL] = 0xE0;
        }
    };
    // Outside the user bank, address 0x0F belongs to another register page.
    device.read_override = [](const FakeDevice& self, std::uint8_t reg, std::uint8_t& value) {
        if (reg == WHO_AM_I && (self.regs[FUNC_CFG_ACCESS] & 0xC0) != 0)
        {
            value = 0x00;
            return true;
        }
        return false;
    };
    return device;
}
} // namespace lsm

namespace lis
{
constexpr std::uint8_t CTRL_REG1 = 0x20;
constexpr std::uint8_t CTRL_REG2 = 0x21;
constexpr std::uint8_t CTRL_REG3 = 0x22;
constexpr std::uint8_t CTRL_REG4 = 0x23;
constexpr std::uint8_t CTRL_REG5 = 0x24;
constexpr std::uint8_t OUT_X_L = 0x28;

FakeDevice make_device()
{
    FakeDevice device;
    device.msb_auto_increment = true;
    device.regs[WHO_AM_I] = imu::Lis3mdlDevice::DEVICE_ID;
    device.regs[CTRL_REG1] = 0x10;
    device.regs[CTRL_REG3] = 0x03;
    device.on_write = [](FakeDevice& self, std::uint8_t reg) {
        if (reg == CTRL_REG2 && (self.regs[CTRL_REG2] & 0x04) != 0)
        {
            self.regs[CTRL_REG1] = 0x10;
            self.regs[CTRL_REG2] = 0x00;
            self.regs[CTRL_REG3] = 0x03;
            self.regs[CTRL_REG4] = 0x00;
            self.regs[CTRL_REG5] = 0x00;
        }
    };
    return device;
}
} // namespace lis

void lsm6dsox_tests()
{
    {
        FakeDevice device = lsm::make_device();
        imu::Lsm6dsoxDevice sensor(device);
        CHECK(sensor.init().has_value());
        CHECK(device.regs[lsm::CTRL1_XL] == 0x40); // 104 Hz, +/-2 g
        CHECK(device.regs[lsm::CTRL2_G] == 0x44);  // 104 Hz, +/-500 dps
        CHECK(device.regs[lsm::CTRL3_C] == 0x44);  // BDU, auto-increment
        CHECK((device.regs[lsm::CTRL9_XL] & 0x02) != 0);
    }
    {
        FakeDevice device = lsm::make_device();
        device.regs[WHO_AM_I] = 0x6B;
        imu::Lsm6dsoxDevice sensor(device);
        const auto result = sensor.init();
        CHECK(!result.has_value() && result.error() == imu::Error::WrongDevice);
        // Only the bank selection, which every LSM6DS-family device shares, was written.
        CHECK(device.writes == 1);
    }
    {
        // Left in the embedded-function bank by an MCU reset in the middle of configuration.
        FakeDevice device = lsm::make_device();
        device.regs[lsm::FUNC_CFG_ACCESS] = 0x80;
        imu::Lsm6dsoxDevice sensor(device);
        CHECK(sensor.init().has_value());
        CHECK(device.regs[lsm::FUNC_CFG_ACCESS] == 0x00);
        CHECK(device.regs[lsm::CTRL1_XL] == 0x40);
    }
    {
        FakeDevice device = lsm::make_device();
        device.fail = true;
        imu::Lsm6dsoxDevice sensor(device);
        const auto result = sensor.init();
        CHECK(!result.has_value() && result.error() == imu::Error::Bus);
    }
    {
        FakeDevice device = lsm::make_device();
        device.on_write = nullptr;
        imu::Lsm6dsoxDevice sensor(device);
        const auto result = sensor.init();
        CHECK(!result.has_value() && result.error() == imu::Error::Timeout);
    }
    {
        FakeDevice device = lsm::make_device();
        device.put16(lsm::OUTX_L_G + 0, 1000);
        device.put16(lsm::OUTX_L_G + 2, -1000);
        device.put16(lsm::OUTX_L_G + 4, 0);
        device.put16(lsm::OUTX_L_A + 0, -8197);
        device.put16(lsm::OUTX_L_A + 2, 0);
        device.put16(lsm::OUTX_L_A + 4, 16393);
        imu::Lsm6dsoxDevice sensor(device);
        const auto sample = sensor.read();
        CHECK(sample.has_value());
        CHECK(near(sample->gyro.x, 0.305433f));
        CHECK(near(sample->gyro.y, -0.305433f));
        CHECK(near(sample->gyro.z, 0.0f));
        CHECK(near(sample->accel.x, -4.9033f));
        CHECK(near(sample->accel.y, 0.0f));
        CHECK(near(sample->accel.z, 9.8064f));
    }
    {
        FakeDevice device = lsm::make_device();
        constexpr auto low = std::numeric_limits<std::int16_t>::min();
        constexpr auto high = std::numeric_limits<std::int16_t>::max();
        device.put16(lsm::OUTX_L_G, low);
        device.put16(lsm::OUTX_L_G + 2, high);
        device.put16(lsm::OUTX_L_A, low);
        device.put16(lsm::OUTX_L_A + 2, high);
        imu::Lsm6dsoxDevice sensor(device);
        const auto sample = sensor.read();
        CHECK(sample.has_value());
        constexpr float gyro_scale = 17.5f * std::numbers::pi_v<float> / 180'000.0f;
        constexpr float accel_scale = 0.061f * 9.80665f / 1000.0f;
        CHECK(near(sample->gyro.x, low * gyro_scale));
        CHECK(near(sample->gyro.y, high * gyro_scale));
        CHECK(near(sample->accel.x, low * accel_scale));
        CHECK(near(sample->accel.y, high * accel_scale));
        CHECK(near(sample->gyro.z, 0.0f) && near(sample->accel.z, 0.0f));
    }
}

void lis3mdl_tests()
{
    {
        FakeDevice device = lis::make_device();
        imu::Lis3mdlDevice sensor(device);
        CHECK(sensor.init().has_value());
        CHECK(device.regs[lis::CTRL_REG1] == 0x7C); // ultra-high performance XY, 80 Hz
        CHECK(device.regs[lis::CTRL_REG2] == 0x00); // +/-4 gauss
        CHECK(device.regs[lis::CTRL_REG3] == 0x00); // continuous conversion
        CHECK(device.regs[lis::CTRL_REG4] == 0x0C); // ultra-high performance Z
        CHECK(device.regs[lis::CTRL_REG5] == 0x40); // BDU
    }
    {
        FakeDevice device = lis::make_device();
        device.regs[WHO_AM_I] = 0x33;
        imu::Lis3mdlDevice sensor(device);
        const auto result = sensor.init();
        CHECK(!result.has_value() && result.error() == imu::Error::WrongDevice);
        CHECK(device.writes == 0);
    }
    {
        FakeDevice device = lis::make_device();
        device.on_write = nullptr;
        imu::Lis3mdlDevice sensor(device);
        const auto result = sensor.init();
        CHECK(!result.has_value() && result.error() == imu::Error::Timeout);
    }
    {
        FakeDevice device = lis::make_device();
        device.put16(lis::OUT_X_L + 0, 6842);
        device.put16(lis::OUT_X_L + 2, -3421);
        device.put16(lis::OUT_X_L + 4, 0);

        std::array<std::uint8_t, 6> without_increment{};
        CHECK(device.read(lis::OUT_X_L, without_increment) == 0);
        CHECK(without_increment[1] == without_increment[0]);

        imu::Lis3mdlDevice sensor(device);
        const auto field = sensor.read();
        CHECK(field.has_value());
        CHECK(near(field->x, 100.0f));
        CHECK(near(field->y, -50.0f));
        CHECK(near(field->z, 0.0f));
    }
    {
        FakeDevice device = lis::make_device();
        constexpr auto low = std::numeric_limits<std::int16_t>::min();
        constexpr auto high = std::numeric_limits<std::int16_t>::max();
        device.put16(lis::OUT_X_L, low);
        device.put16(lis::OUT_X_L + 2, high);
        imu::Lis3mdlDevice sensor(device);
        const auto field = sensor.read();
        CHECK(field.has_value());
        CHECK(near(field->x, low * 100.0f / 6842.0f));
        CHECK(near(field->y, high * 100.0f / 6842.0f));
        CHECK(near(field->z, 0.0f));
    }
}
} // namespace

int main()
{
    lsm6dsox_tests();
    lis3mdl_tests();
    return EXIT_SUCCESS;
}
