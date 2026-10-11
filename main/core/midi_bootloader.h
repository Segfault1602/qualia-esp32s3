#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

#include "midi_message.h"

namespace midi
{
class BootloaderCommand
{
  public:
    // Non-commercial manufacturer ID, "QUALIA", command 01.
    static constexpr std::array<std::uint8_t, 10> MESSAGE{0xF0, 0x7D, 0x51, 0x55, 0x41, 0x4C, 0x49, 0x41, 0x01, 0xF7};

    void reset()
    {
        position_ = 0;
    }

    bool feed_message(const Message& message, std::chrono::microseconds now)
    {
        if (!message.valid())
        {
            reset();
            return false;
        }
        if (!message.ump)
        {
            return feed(message.packet(), now);
        }
        if (position_ != 0 && !ump_)
        {
            reset();
        }
        ump_ = true;
        const auto type = message.words[0] >> 28;
        const auto group = (message.words[0] >> 24) & 0xF;
        if (group != 0)
        {
            return false;
        }
        if (type == 1 && ((message.words[0] >> 16) & 0xFF) >= 0xF8)
        {
            return false;
        }
        if (type != 3)
        {
            reset();
            return false;
        }
        if (position_ != 0 && now - last_byte_ > std::chrono::seconds(1))
        {
            reset();
        }
        const auto status = (message.words[0] >> 20) & 0xF;
        const auto count = (message.words[0] >> 16) & 0xF;
        if (status > 3 || count > 6)
        {
            reset();
            return false;
        }
        if (status <= 1)
        {
            position_ = 1;
            last_byte_ = now;
        }
        else if (position_ == 0)
        {
            return false;
        }
        for (std::uint32_t i = 0; i < count; ++i)
        {
            const auto byte_index = i + 2;
            const auto byte = (message.words[byte_index / 4] >> (24 - 8 * (byte_index % 4))) & 0xFF;
            if (position_ >= MESSAGE.size() - 1 || byte != MESSAGE[position_])
            {
                reset();
                return false;
            }
            ++position_;
            last_byte_ = now;
        }
        const bool ending = status == 0 || status == 3;
        if (ending)
        {
            const bool match = position_ == MESSAGE.size() - 1;
            reset();
            return match;
        }
        return false;
    }

    bool feed(const std::array<std::uint8_t, 4>& packet, std::chrono::microseconds now)
    {
        if (position_ != 0 && ump_)
        {
            reset();
        }
        ump_ = false;
        if (packet[0] >> 4 != 0)
        {
            return false;
        }
        if (position_ != 0 && now - last_byte_ > std::chrono::seconds(1))
        {
            reset();
        }
        const auto cin = packet[0] & 0x0F;
        if (cin == 0x0F && packet[1] >= 0xF8)
        {
            return false;
        }
        if (cin < 4 || cin > 7)
        {
            reset();
            return false;
        }
        const int count = cin == 4 ? 3 : cin - 4;
        for (int i = 1; i <= count; ++i)
        {
            const auto byte = packet[i];
            if (byte == 0xF0)
            {
                if (i != 1 || cin != 4)
                {
                    reset();
                    return false;
                }
                position_ = 1;
                last_byte_ = now;
                continue;
            }
            if (position_ == 0 || byte != MESSAGE[position_])
            {
                reset();
                return false;
            }
            ++position_;
            last_byte_ = now;
            if (position_ == MESSAGE.size())
            {
                reset();
                return cin != 4 && i == count;
            }
        }
        if (cin != 4)
        {
            reset();
        }
        return false;
    }

  private:
    std::size_t position_ = 0;
    std::chrono::microseconds last_byte_{};
    bool ump_ = false;
};
} // namespace midi
