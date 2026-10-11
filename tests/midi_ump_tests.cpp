#include "midi_bootloader.h"
#include "midi_endpoint.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>

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
using namespace std::chrono_literals;
constexpr midi::Message START{{0x30167D51, 0x55414C49}, 2, true};
constexpr midi::Message END{{0x30324101, 0}, 2, true};

void codec_tests()
{
    constexpr std::array<std::uint8_t, 16> lengths{1, 1, 1, 2, 2, 4, 1, 1, 2, 2, 2, 3, 3, 4, 4, 4};
    for (std::uint32_t type = 0; type < lengths.size(); ++type)
    {
        CHECK(midi::ump_word_count(type << 28) == lengths[type]);
        CHECK((midi::Message{{type << 28}, lengths[type], true}.valid()));
        CHECK((!midi::Message{{type << 28}, static_cast<std::uint8_t>(lengths[type] - 1), true}.valid()));
    }
    const midi::Note note{60, 0.5};
    const auto legacy = midi::note_message(note, true, midi::Format::UsbMidi1);
    CHECK((legacy.packet() == midi::UsbPacket{9, 0x90, 60, 64}));
    CHECK(legacy.valid());
    const auto ump1 = midi::note_message(note, true, midi::Format::UmpMidi1);
    CHECK(ump1.size == 1 && ump1.words[0] == 0x20903C40);
    const auto ump2 = midi::note_message(note, true, midi::Format::UmpMidi2);
    CHECK(ump2.size == 2 && ump2.words[0] == 0x40903C00 && ump2.words[1] == 0x80000000);
    CHECK((ump2.packet() == midi::UsbPacket{0, 0x3C, 0x90, 0x40}));
    const auto off = midi::note_message(note, false, midi::Format::UmpMidi2);
    CHECK(off.words[0] == 0x40803C00 && off.words[1] == 0);
    CHECK(midi::note_message(note, false, midi::Format::UmpMidi1).words[0] == 0x20803C00);
}

void sysex_tests()
{
    midi::BootloaderCommand parser;
    for (unsigned splits = 1; splits < 128; ++splits)
    {
        std::array<midi::Message, 8> messages{};
        unsigned size = 0;
        unsigned begin = 0;
        bool valid = true;
        for (unsigned end = 1; end <= 8; ++end)
        {
            if (end != 8 && !(splits & (1U << (end - 1))))
            {
                continue;
            }
            const auto count = end - begin;
            valid = valid && count <= 6;
            const unsigned status = begin == 0 ? 1 : end == 8 ? 3 : 2;
            auto& message = messages[size++];
            message = {{0x30000000U | (status << 20) | (count << 16)}, 2, true};
            for (unsigned i = 0; i < count; ++i)
            {
                const auto byte = i + 2;
                message.words[byte / 4] |= static_cast<std::uint32_t>(midi::BootloaderCommand::MESSAGE[begin + i + 1])
                                           << (24 - 8 * (byte % 4));
            }
            begin = end;
        }
        if (!valid)
        {
            continue;
        }
        parser.reset();
        for (unsigned i = 0; i < size; ++i)
        {
            CHECK(parser.feed_message(messages[i], 0us) == (i == size - 1));
        }
    }
    CHECK(!parser.feed_message({{0x30100000, 0}, 2, true}, 2s));
    auto continuation = START;
    continuation.words[0] = 0x30267D51;
    CHECK(!parser.feed_message(continuation, 2500ms));
    CHECK(parser.feed_message(END, 2900ms));
    CHECK(!parser.feed_message(START, 0us));
    CHECK(!parser.feed_message({{0x30224101, 0}, 2, true}, 0us));
    CHECK(parser.feed_message({{0x30300000, 0}, 2, true}, 0us));
    CHECK(!parser.feed_message(START, 0us));
    CHECK(!parser.feed({5, 0xF7, 0, 0}, 0us));
    CHECK(!parser.feed_message(END, 0us));
    CHECK(!parser.feed({4, 0xF0, 0x7D, 0x51}, 0us));
    CHECK(!parser.feed_message(END, 0us));
    for (std::uint32_t type = 0; type < 16; ++type)
    {
        for (std::uint32_t status = 0; status < 16; ++status)
        {
            for (std::uint32_t count = 0; count < 16; ++count)
            {
                parser.reset();
                CHECK(!parser.feed_message(START, 0us));
                const auto word = (type << 28) | (status << 20) | (count << 16);
                CHECK(!parser.feed_message({{word, 0, 0, 0}, midi::ump_word_count(word), true}, 0us));
            }
        }
    }
    parser.reset();
    CHECK(!parser.feed_message(START, 0us));
    CHECK(parser.feed_message(END, 0us));
    CHECK(!parser.feed_message(END, 0us));
    for (unsigned index = 0; index < 8; ++index)
    {
        parser.reset();
        auto start = START;
        auto end = END;
        auto& message = index < 6 ? start : end;
        const auto byte = index < 6 ? index + 2 : index - 6 + 2;
        message.words[byte / 4] ^= 1U << (24 - 8 * (byte % 4));
        CHECK(!parser.feed_message(start, 0us));
        CHECK(!parser.feed_message(end, 0us));
    }
    for (auto word : {0x30177D51U, 0x30467D51U, 0x30067D51U})
    {
        parser.reset();
        auto bad = START;
        bad.words[0] = word;
        CHECK(!parser.feed_message(bad, 0us));
        CHECK(!parser.feed_message(END, 0us));
    }
    CHECK(!parser.feed_message(START, 0us));
    CHECK(!parser.feed_message({{0x10F80000}, 1, true}, 500ms));
    auto other_group = START;
    other_group.words[0] |= 0x01000000;
    CHECK(!parser.feed_message(other_group, 500ms));
    CHECK(parser.feed_message(END, 900ms));
    CHECK(!parser.feed_message(START, 0us));
    CHECK(!parser.feed_message(END, 2s));
    CHECK(!parser.feed_message(START, 0us));
    CHECK(!parser.feed_message({{0x20903C40}, 1, true}, 0us));
    CHECK(!parser.feed_message(END, 0us));
    CHECK(!parser.feed_message(START, 0us));
    CHECK(!parser.feed_message({{0x50100000}, 4, true}, 0us));
    CHECK(!parser.feed_message(END, 0us));
    CHECK(!parser.feed_message(START, 0us));
    CHECK(!parser.feed_message({{0x30324101}, 1, true}, 0us));
    CHECK(!parser.feed_message(END, 0us));
}

void endpoint_tests()
{
    midi::Endpoint endpoint;
    CHECK(!endpoint.pending());
    CHECK(!endpoint.next("123456789ABC"));
    CHECK(endpoint.protocol() == midi::Format::UmpMidi2);
    for (unsigned bit = 0; bit < 5; ++bit)
    {
        CHECK(endpoint.receive({{0xF0000101, 1U << bit, 0, 0}, 4, true}));
        const auto message = endpoint.next("123456789ABC");
        CHECK(message && message->valid());
        constexpr std::array<unsigned, 5> statuses{1, 2, 3, 4, 6};
        CHECK(((message->words[0] >> 16) & 0x3FF) == statuses[bit]);
        endpoint.sent();
        CHECK(!endpoint.pending());
    }
    constexpr std::array<midi::Message, 5> notifications{
        midi::Message{{0xF0010101, 0x81000300, 0, 0}, 4, true},
        midi::Message{{0xF0020000, 0x007D0000, 0x01000100, 0x00000001}, 4, true},
        midi::Message{{0xF0035175, 0x616C6961, 0x204D4944, 0x49320000}, 4, true},
        midi::Message{{0xF0043132, 0x33343536, 0x37383941, 0x42430000}, 4, true},
        midi::Message{{0xF0060200, 0, 0, 0}, 4, true},
    };
    for (unsigned bit = 0; bit < notifications.size(); ++bit)
    {
        endpoint.receive({{0xF0000101, 1U << bit, 0, 0}, 4, true});
        CHECK(endpoint.next("123456789ABC") == notifications[bit]);
        endpoint.sent();
    }
    for (std::uint32_t filter = 0; filter < 256; ++filter)
    {
        endpoint.reset();
        endpoint.receive({{0xF0000101, filter, 0, 0}, 4, true});
        unsigned replies = 0;
        for (unsigned bit = 0; bit < 5; ++bit)
        {
            replies += static_cast<bool>(filter & (1U << bit));
        }
        for (unsigned i = 0; i < replies; ++i)
        {
            CHECK(endpoint.pending());
            endpoint.sent();
        }
        CHECK(!endpoint.pending());
        endpoint.receive({{0xF0100000 | filter, 0, 0, 0}, 4, true});
        for (unsigned bit = 0; bit < 2; ++bit)
        {
            if (filter & (1U << bit))
            {
                CHECK(endpoint.pending());
                endpoint.sent();
            }
        }
        CHECK(!endpoint.pending());
    }
    endpoint.reset();
    CHECK(endpoint.receive({{0xF0000101, 0x1F, 0, 0}, 4, true}));
    CHECK(endpoint.receive({{0xF010FF03, 0, 0, 0}, 4, true}));
    const auto first = endpoint.next("123456789ABC");
    CHECK(first && first->words[0] == 0xF0060200);
    CHECK(endpoint.next("123456789ABC") == first);
    for (int i = 0; i < 7; ++i)
    {
        CHECK(endpoint.next("123456789ABC"));
        endpoint.sent();
    }
    CHECK(!endpoint.pending());
    CHECK(endpoint.receive({{0xF0100001, 0, 0, 0}, 4, true}));
    CHECK(endpoint.next("123456789ABC")->words[0] == 0xF0118033);
    CHECK(endpoint.next("123456789ABC")->words[1] == 0x00010000);
    endpoint.sent();
    CHECK(endpoint.receive({{0xF0100103, 0, 0, 0}, 4, true}));
    CHECK(!endpoint.pending());
    CHECK(endpoint.receive({{0xF0050100, 0, 0, 0}, 4, true}));
    CHECK(endpoint.protocol() == midi::Format::UmpMidi1);
    CHECK(endpoint.next("123456789ABC")->words[0] == 0xF0060100);
    endpoint.sent();
    CHECK(endpoint.receive({{0xF0057F00, 0, 0, 0}, 4, true}));
    CHECK(endpoint.protocol() == midi::Format::UmpMidi1);
    CHECK(endpoint.next("123456789ABC")->words[0] == 0xF0060100);
    endpoint.reset();
    CHECK(endpoint.protocol() == midi::Format::UmpMidi2 && !endpoint.pending());
    CHECK(!endpoint.receive({{0xF0000101, 0x1F}, 2, true}));
    CHECK(!endpoint.receive({{0xF4000101, 0x1F}, 4, true}));
}
} // namespace

int main()
{
    codec_tests();
    sysex_tests();
    endpoint_tests();
    return EXIT_SUCCESS;
}
