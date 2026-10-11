#include "midi_handoff.h"
#include "modules/midi.h"
#include "modules/module.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <vector>

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
using Packet = drivers::MidiUsb::Packet;

struct FakeUsb
{
    static inline bool online = true;
    static inline bool fail_init = false;
    static inline bool flood = false;
    static inline int write_budget = -1;
    static inline int reads = 0;
    static inline std::vector<Packet> sent;
    static inline std::deque<Packet> received;
    static inline midi::State current_state{true, midi::Format::UsbMidi1, 1};
    static inline std::vector<midi::Message> messages_sent;
    static inline std::deque<midi::Message> messages_received;
    static inline bool defer = false;
    static inline bool read_failure = false;
    static inline bool change_session_on_read = false;

    static void reset()
    {
        online = true;
        fail_init = false;
        flood = false;
        write_budget = -1;
        reads = 0;
        sent.clear();
        received.clear();
        current_state = {true, midi::Format::UsbMidi1, 1};
        messages_sent.clear();
        messages_received.clear();
        defer = false;
        read_failure = false;
        change_session_on_read = false;
    }
    static esp_err_t init()
    {
        return fail_init ? ESP_FAIL : ESP_OK;
    }
    static bool connected()
    {
        return online;
    }
    static midi::State state()
    {
        if (current_state.connected != online)
        {
            current_state.connected = online;
            ++current_state.generation;
        }
        return current_state;
    }
    static std::expected<void, esp_err_t> write_message(const midi::Message& message, midi::State expected_state)
    {
        CHECK(expected_state == state());
        if (defer)
        {
            return std::unexpected(ESP_ERR_NOT_FINISHED);
        }
        const auto result = write_packet(message.packet());
        if (result)
        {
            messages_sent.push_back(message);
        }
        return result;
    }
    static std::expected<std::optional<midi::Message>, esp_err_t> read_message()
    {
        if (change_session_on_read)
        {
            ++current_state.generation;
            change_session_on_read = false;
        }
        if (read_failure)
        {
            return std::unexpected(ESP_ERR_NO_MEM);
        }
        if (!messages_received.empty())
        {
            ++reads;
            const auto message = messages_received.front();
            messages_received.pop_front();
            return message;
        }
        const auto packet = read_packet();
        return packet ? std::optional{midi::Message::legacy(*packet)} : std::nullopt;
    }
    static std::expected<void, esp_err_t> write_packet(const Packet& packet)
    {
        if (!online)
        {
            return std::unexpected(ESP_ERR_INVALID_STATE);
        }
        if (write_budget == 0)
        {
            return std::unexpected(ESP_ERR_TIMEOUT);
        }
        if (write_budget > 0)
        {
            --write_budget;
        }
        sent.push_back(packet);
        return {};
    }
    static std::optional<Packet> read_packet()
    {
        ++reads;
        if (!received.empty())
        {
            const auto packet = received.front();
            received.pop_front();
            return packet;
        }
        return flood ? std::optional<Packet>({0x09, 0x90, 60, 80}) : std::nullopt;
    }
};

using Midi = modules::MidiController<FakeUsb>;
static_assert(modules::Module<Midi>);

void init(Midi& midi)
{
    FakeUsb::reset();
    CHECK(!midi.is_initialized());
    CHECK(midi.init() == ESP_OK);
    CHECK(midi.init() == ESP_ERR_INVALID_STATE);
    midi.step(0us);
    CHECK(midi.can_submit());
}

void packet(std::size_t index, Packet expected)
{
    CHECK(index < FakeUsb::sent.size());
    CHECK(FakeUsb::sent[index] == expected);
}

constexpr std::array<Packet, 4> command_packets{Packet{0x04, 0xF0, 0x7D, 0x51}, Packet{0x04, 0x55, 0x41, 0x4C},
                                                Packet{0x04, 0x49, 0x41, 0x01}, Packet{0x05, 0xF7, 0, 0}};

void note_tests()
{
    {
        Midi midi;
        FakeUsb::reset();
        FakeUsb::fail_init = true;
        CHECK(midi.init() == ESP_FAIL);
        CHECK(!midi.is_initialized());
        CHECK(midi.submit({60, 90.0 / 127.0}) == ESP_ERR_INVALID_STATE);
    }
    {
        Midi midi;
        init(midi);
        CHECK(midi.submit({128, 80.0 / 127.0}) == ESP_ERR_INVALID_ARG);
        CHECK(midi.submit({60, 0.0 / 127.0}) == ESP_ERR_INVALID_ARG);
        CHECK(midi.submit({60, 128.0 / 127.0}) == ESP_ERR_INVALID_ARG);
        CHECK(midi.submit({60, 64.0 / 127.0}) == ESP_OK);
        CHECK(midi.submit({62, 64.0 / 127.0}) == ESP_ERR_INVALID_STATE);
        midi.step(0us);
        packet(0, {0x09, 0x90, 60, 64});
        midi.step(99ms);
        CHECK(FakeUsb::sent.size() == 1);
        midi.step(100ms);
        packet(1, {0x08, 0x80, 60, 0});
        midi.step(200ms);
        CHECK(FakeUsb::sent.size() == 2);
    }
    {
        Midi midi;
        init(midi);
        FakeUsb::write_budget = 0;
        CHECK(midi.submit({48, 127.0 / 127.0}) == ESP_OK);
        midi.step(0us);
        CHECK(FakeUsb::sent.empty());
        CHECK(!midi.can_submit());
        FakeUsb::write_budget = -1;
        midi.step(20ms);
        packet(0, {0x09, 0x90, 48, 127});
        midi.step(119ms);
        CHECK(FakeUsb::sent.size() == 1);
        FakeUsb::write_budget = 0;
        midi.step(120ms);
        CHECK(FakeUsb::sent.size() == 1);
        CHECK(midi.submit({72, 1.0 / 127.0}) == ESP_OK);
        midi.step(121ms);
        CHECK(FakeUsb::sent.size() == 1);
        FakeUsb::write_budget = -1;
        midi.step(122ms);
        packet(1, {0x08, 0x80, 48, 0});
        packet(2, {0x09, 0x90, 72, 1});
    }
    {
        Midi midi;
        init(midi);
        CHECK(midi.submit({60, 90.0 / 127.0}) == ESP_OK);
        midi.step(0us);
        CHECK(midi.submit({62, 100.0 / 127.0}) == ESP_OK);
        midi.step(10ms);
        CHECK(midi.submit({60, 110.0 / 127.0}) == ESP_OK);
        FakeUsb::write_budget = 0;
        midi.step(20ms);
        CHECK(FakeUsb::sent.size() == 2);
        FakeUsb::write_budget = 1;
        midi.step(25ms);
        packet(2, {0x08, 0x80, 60, 0});
        CHECK(FakeUsb::sent.size() == 3);
        FakeUsb::write_budget = -1;
        midi.step(30ms);
        packet(3, {0x09, 0x90, 60, 110});
        midi.step(100ms);
        CHECK(FakeUsb::sent.size() == 4);
        midi.step(110ms);
        packet(4, {0x08, 0x80, 62, 0});
        midi.step(130ms);
        packet(5, {0x08, 0x80, 60, 0});
    }
    {
        Midi midi;
        init(midi);
        CHECK(midi.submit({60, 100.0 / 127.0}) == ESP_OK);
        midi.step(0us);
        CHECK(midi.submit({62, 100.0 / 127.0}) == ESP_OK);
        FakeUsb::online = false;
        midi.step(1ms);
        CHECK(!midi.can_submit());
        FakeUsb::online = true;
        FakeUsb::write_budget = 0;
        midi.step(2ms);
        CHECK(!midi.ready_for_notes());
        FakeUsb::write_budget = -1;
        midi.step(3ms);
        packet(1, {0x08, 0x80, 60, 0});
        CHECK(FakeUsb::sent.size() == 2);
        CHECK(midi.can_submit());
        FakeUsb::flood = true;
        FakeUsb::reads = 0;
        CHECK(midi.submit({72, 100.0 / 127.0}) == ESP_OK);
        midi.step(4ms);
        CHECK(FakeUsb::reads == 16);
        midi.step(104ms);
        packet(3, {0x08, 0x80, 72, 0});
    }
}

void sysex_tests()
{
    midi::BootloaderCommand parser;
    for (std::size_t i = 0; i < command_packets.size(); ++i)
    {
        CHECK(parser.feed(command_packets[i], 0us) == (i == 3));
    }
    for (std::size_t position = 0; position < midi::BootloaderCommand::MESSAGE.size(); ++position)
    {
        parser.reset();
        auto wrong = command_packets;
        wrong[position / 3][position % 3 + 1] ^= 1;
        for (const auto& p : wrong)
        {
            CHECK(!parser.feed(p, 0us));
        }
    }
    CHECK(!parser.feed(command_packets[0], 0us));
    CHECK(!parser.feed({0x0F, 0xF8, 0, 0}, 0us));
    CHECK(!parser.feed({0x14, 0xF0, 0x7D, 0x51}, 0us));
    CHECK(!parser.feed(command_packets[1], 0us));
    CHECK(!parser.feed(command_packets[2], 0us));
    CHECK(parser.feed(command_packets[3], 0us));
    CHECK(!parser.feed(command_packets[0], 0us));
    for (std::size_t i = 1; i < command_packets.size(); ++i)
    {
        CHECK(!parser.feed(command_packets[i], 2s));
    }
    CHECK(!parser.feed(command_packets[0], 0us));
    CHECK(!parser.feed({0x09, 0x90, 60, 90}, 0us));
    for (std::size_t i = 1; i < command_packets.size(); ++i)
    {
        CHECK(!parser.feed(command_packets[i], 0us));
    }
    for (std::uint8_t ending : {4, 6, 7})
    {
        parser.reset();
        for (std::size_t i = 0; i < 3; ++i)
        {
            CHECK(!parser.feed(command_packets[i], 0us));
        }
        CHECK(!parser.feed({ending, 0xF7, 0, 0}, 0us));
    }
    // A SysEx start in the middle of a USB event packet is not valid framing.
    CHECK(!parser.feed({0x04, 0x55, 0xF0, 0x7D}, 0us));
    CHECK(!parser.feed({0x04, 0x51, 0x55, 0x41}, 0us));
    CHECK(!parser.feed({0x04, 0x4C, 0x49, 0x41}, 0us));
    CHECK(!parser.feed({0x06, 0x01, 0xF7, 0}, 0us));
    {
        Midi midi;
        init(midi);
        CHECK(midi.submit({60, 90.0 / 127.0}) == ESP_OK);
        midi.step(0us);
        FakeUsb::received.assign(command_packets.begin(), command_packets.end());
        midi.step(1ms);
        CHECK(midi.bootloader_requested());
        CHECK(!midi.can_submit());
        CHECK(!midi.bootloader_ready());
        FakeUsb::write_budget = 0;
        midi.step(2ms);
        CHECK(!midi.bootloader_ready());
        FakeUsb::write_budget = -1;
        midi.step(3ms);
        CHECK(midi.bootloader_ready());
        packet(1, {0x08, 0x80, 60, 0});
    }
    {
        Midi midi;
        init(midi);
        FakeUsb::received.push_back(command_packets[0]);
        midi.step(0us);
        FakeUsb::online = false;
        midi.step(1ms);
        FakeUsb::online = true;
        for (std::size_t i = 1; i < command_packets.size(); ++i)
        {
            FakeUsb::received.push_back(command_packets[i]);
        }
        midi.step(2ms);
        CHECK(!midi.bootloader_requested());
    }
}

void ump_module_tests()
{
    Midi midi;
    init(midi);
    FakeUsb::current_state.format = midi::Format::UmpMidi2;
    ++FakeUsb::current_state.generation;
    midi.step(1ms);
    CHECK(midi.submit({60, 0.5}) == ESP_OK);
    FakeUsb::defer = true;
    midi.step(2ms);
    midi.step(200ms);
    CHECK(FakeUsb::messages_sent.empty());
    FakeUsb::defer = false;
    midi.step(201ms);
    CHECK(FakeUsb::messages_sent[0].words[0] == 0x40903C00);
    CHECK(FakeUsb::messages_sent[0].words[1] == 0x80000000);
    midi.step(300ms);
    CHECK(FakeUsb::messages_sent.size() == 1);
    midi.step(301ms);
    CHECK(FakeUsb::messages_sent[1].words[0] == 0x40803C00);
    CHECK(midi.submit({62, 0.5}) == ESP_OK);
    midi.step(302ms);
    CHECK(midi.submit({64, 0.5}) == ESP_OK);
    FakeUsb::current_state.format = midi::Format::UmpMidi1;
    ++FakeUsb::current_state.generation;
    midi.step(303ms);
    CHECK(FakeUsb::messages_sent.back().words[0] == 0x20803E00);
    CHECK(midi.can_submit());
    CHECK(midi.submit({65, 0.5}) == ESP_OK);
    FakeUsb::defer = true;
    midi.step(304ms);
    FakeUsb::current_state.format = midi::Format::UsbMidi1;
    ++FakeUsb::current_state.generation;
    FakeUsb::defer = false;
    midi.step(305ms);
    CHECK((FakeUsb::sent.back() == Packet{8, 0x80, 65, 0}));
    CHECK(midi.can_submit());
    const midi::Message start{{0x30167D51, 0x55414C49}, 2, true};
    const midi::Message end{{0x30324101, 0}, 2, true};
    FakeUsb::messages_received.push_back(start);
    midi.step(306ms);
    FakeUsb::read_failure = true;
    midi.step(307ms);
    FakeUsb::read_failure = false;
    FakeUsb::messages_received.push_back(end);
    midi.step(308ms);
    CHECK(!midi.bootloader_requested());
    FakeUsb::messages_received.push_back(start);
    midi.step(309ms);
    FakeUsb::messages_received.push_back(end);
    FakeUsb::change_session_on_read = true;
    midi.step(310ms);
    CHECK(!midi.bootloader_requested());
    FakeUsb::messages_received.push_back(start);
    FakeUsb::messages_received.push_back(end);
    midi.step(311ms);
    CHECK(midi.bootloader_requested());
    midi.step(312ms);
    CHECK(midi.bootloader_ready());
}

app::MidiHandoff* racing_handoff = nullptr;

void reconnect_during_publish()
{
    CHECK(racing_handoff->set_online(false) == ESP_OK);
    CHECK(racing_handoff->set_online(true) == ESP_OK);
}

void handoff_tests()
{
    app::MidiHandoff handoff;
    CHECK(handoff.set_online(true) == ESP_ERR_INVALID_STATE);
    CHECK(handoff.init() == ESP_OK);
    CHECK(handoff.init() == ESP_ERR_INVALID_STATE);
    CHECK(handoff.publish({60, 80.0 / 127.0}) == ESP_ERR_INVALID_STATE);
    CHECK(handoff.set_online(true) == ESP_OK);
    for (unsigned i = 0; i < app::MidiHandoff::CAPACITY; ++i)
    {
        CHECK(handoff.publish({static_cast<std::uint8_t>(48 + i), 80.0 / 127.0}) == ESP_OK);
    }
    CHECK(handoff.publish({60, 80.0 / 127.0}) == ESP_ERR_NO_MEM);
    for (unsigned i = 0; i < app::MidiHandoff::CAPACITY; ++i)
    {
        const auto note = handoff.receive();
        CHECK(note && note->pitch == 48 + i);
    }
    CHECK(!handoff.receive());
    CHECK(handoff.publish({60, 80.0 / 127.0}) == ESP_OK);
    CHECK(handoff.set_online(false) == ESP_OK);
    CHECK(handoff.set_online(true) == ESP_OK);
    CHECK(!handoff.receive());
    racing_handoff = &handoff;
    host_queue_before_send = reconnect_during_publish;
    CHECK(handoff.publish({60, 80.0 / 127.0}) == ESP_OK);
    CHECK(!handoff.receive());
    CHECK(handoff.publish({62, 80.0 / 127.0}) == ESP_OK);
    CHECK(handoff.receive()->pitch == 62);
}
} // namespace

int main()
{
    note_tests();
    sysex_tests();
    ump_module_tests();
    handoff_tests();
    return EXIT_SUCCESS;
}
