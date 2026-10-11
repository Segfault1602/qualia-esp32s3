#pragma once

#include <array>
#include <cstdint>

#include "midi_note.h"

namespace midi
{
using UsbPacket = std::array<std::uint8_t, 4>;

enum class Format
{
    UsbMidi1,
    UmpMidi1,
    UmpMidi2,
};

struct State
{
    bool connected = false;
    Format format = Format::UsbMidi1;
    std::uint32_t generation = 0;
    bool operator==(const State&) const = default;
};

constexpr std::uint8_t ump_word_count(std::uint32_t first)
{
    constexpr std::array<std::uint8_t, 16> lengths{1, 1, 1, 2, 2, 4, 1, 1, 2, 2, 2, 3, 3, 4, 4, 4};
    return lengths[first >> 28];
}

struct Message
{
    std::array<std::uint32_t, 4> words{};
    std::uint8_t size = 0;
    bool ump = true;

    bool operator==(const Message&) const = default;

    constexpr bool valid() const
    {
        return size == (ump ? ump_word_count(words[0]) : 1);
    }

    constexpr UsbPacket packet() const
    {
        return {static_cast<std::uint8_t>(words[0]), static_cast<std::uint8_t>(words[0] >> 8),
                static_cast<std::uint8_t>(words[0] >> 16), static_cast<std::uint8_t>(words[0] >> 24)};
    }

    static constexpr Message legacy(const UsbPacket& packet)
    {
        return {{{static_cast<std::uint32_t>(packet[0]) | (static_cast<std::uint32_t>(packet[1]) << 8) |
                  (static_cast<std::uint32_t>(packet[2]) << 16) | (static_cast<std::uint32_t>(packet[3]) << 24)}},
                1,
                false};
    }
};

inline Message note_message(Note note, bool on, Format format)
{
    const std::uint8_t status = on ? 0x90 : 0x80;
    if (format == Format::UsbMidi1)
    {
        return Message::legacy({static_cast<std::uint8_t>(on ? 9 : 8), status, note.pitch,
                                static_cast<std::uint8_t>(on ? velocity7(note) : 0)});
    }
    if (format == Format::UmpMidi1)
    {
        return {{{0x20000000U | (static_cast<std::uint32_t>(status) << 16) |
                  (static_cast<std::uint32_t>(note.pitch) << 8) | (on ? velocity7(note) : 0U)}},
                1,
                true};
    }
    return {{{0x40000000U | (static_cast<std::uint32_t>(status) << 16) | (static_cast<std::uint32_t>(note.pitch) << 8),
              on ? static_cast<std::uint32_t>(velocity16(note)) << 16 : 0U}},
            2,
            true};
}
} // namespace midi
