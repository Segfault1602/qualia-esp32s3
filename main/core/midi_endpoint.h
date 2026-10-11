#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "midi_message.h"

namespace midi
{
class Endpoint
{
  public:
    static constexpr std::string_view NAME = "Qualia MIDI2";
    static constexpr std::string_view BLOCK_NAME = "Bounce";

    void reset()
    {
        protocol_ = Format::UmpMidi2;
        pending_ = 0;
    }

    Format protocol() const
    {
        return protocol_;
    }

    bool receive(const Message& message)
    {
        if (!message.ump || !message.valid() || message.words[0] >> 26 != 0x3C)
        {
            return false;
        }
        const auto status = (message.words[0] >> 16) & 0x3FF;
        if (status == 0)
        {
            pending_ |= message.words[1] & 0x1F;
        }
        else if (status == 0x10)
        {
            const auto block = (message.words[0] >> 8) & 0xFF;
            if (block == 0 || block == 0xFF)
            {
                pending_ |= (message.words[0] & 3) << 5;
            }
        }
        else if (status == 5)
        {
            const auto requested = (message.words[0] >> 8) & 0xFF;
            if (requested == 1 || requested == 2)
            {
                protocol_ = requested == 1 ? Format::UmpMidi1 : Format::UmpMidi2;
            }
            pending_ |= 0x10;
        }
        return true;
    }

    std::optional<Message> next(std::string_view instance) const
    {
        if (pending_ == 0)
        {
            return std::nullopt;
        }
        Message message{{}, 4, true};
        switch (next_bit())
        {
        case 1:
            message.words = {0xF0010101, 0x81000300, 0, 0};
            break;
        case 2:
            message.words = {0xF0020000, 0x007D0000, 0x01000100, 0x00000001};
            break;
        case 4:
            message = text(3, NAME);
            break;
        case 8:
            message = text(4, instance);
            break;
        case 16:
            message.words[0] = 0xF0060000 | (protocol_ == Format::UmpMidi1 ? 0x100U : 0x200U);
            break;
        case 32:
            message.words = {0xF0118033, 0x00010000, 0, 0};
            break;
        case 64:
            message = text(0x12, BLOCK_NAME, true);
            break;
        }
        return message;
    }

    void sent()
    {
        pending_ &= ~next_bit();
    }

    bool pending() const
    {
        return pending_ != 0;
    }

  private:
    std::uint32_t next_bit() const
    {
        if (pending_ & 0x10)
        {
            return 0x10;
        }
        return pending_ & (0U - pending_);
    }

    static Message text(std::uint32_t status, std::string_view value, bool block = false)
    {
        Message message{{{0xF0000000 | (status << 16)}}, 4, true};
        const std::size_t start = block ? 3 : 2;
        for (std::size_t i = 0; i < value.size() && start + i < 16; ++i)
        {
            const auto byte = start + i;
            message.words[byte / 4] |= static_cast<std::uint32_t>(static_cast<unsigned char>(value[i]))
                                       << (24 - 8 * (byte % 4));
        }
        return message;
    }

    Format protocol_ = Format::UmpMidi2;
    std::uint32_t pending_ = 0;
};
} // namespace midi
