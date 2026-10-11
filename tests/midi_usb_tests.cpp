#include "midi_usb.h"
#include "modules/midi.h"

#include <array>
#include <bit>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <vector>

#include <device/dcd.h>
#include <device/usbd_pvt.h>
#include <esp_mac.h>
#include <midi2_device.h>
#include <tinyusb.h>

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                                       \
            std::exit(EXIT_FAILURE);                                                                                   \
        }                                                                                                              \
    } while (false)

extern "C" void tud_sof_cb(std::uint32_t);

namespace
{
using usb = drivers::MidiUsb;
struct Transfer
{
    std::uint8_t* data = nullptr;
    std::uint16_t size = 0;
    bool active = false;
};
tinyusb_config_t config;
std::array<Transfer, 256> transfers{};
std::vector<std::uint8_t> output;
std::vector<std::uint8_t> control;
void (*during_send)() = nullptr;

void event(tinyusb_event_id_t id)
{
    tinyusb_event_t value{id};
    config.event_cb(&value, nullptr);
}

void complete()
{
    auto& in = transfers[0x81];
    CHECK(in.active);
    const auto size = in.size;
    if (size != 0)
    {
        output.insert(output.end(), in.data, in.data + size);
    }
    in.active = false;
    CHECK(midi2d_xfer_cb(0, 0x81, XFER_RESULT_SUCCESS, size));
}

void drain()
{
    for (unsigned i = 0; i < 100 && transfers[0x81].active; ++i)
    {
        complete();
    }
    CHECK(!transfers[0x81].active);
}

void connect()
{
    tud_event_hook_cb(0, DCD_EVENT_BUS_RESET, true);
    midi2d_reset(0);
    transfers = {};
    const auto* descriptor = config.descriptor.full_speed_config;
    const auto length = static_cast<std::uint16_t>(descriptor[2] | descriptor[3] << 8);
    CHECK(midi2d_open(0, reinterpret_cast<const tusb_desc_interface_t*>(descriptor + TUD_CONFIG_DESC_LEN),
                      length - TUD_CONFIG_DESC_LEN) == length - TUD_CONFIG_DESC_LEN);
    event(TINYUSB_EVENT_ATTACHED);
    CHECK(usb::state().connected && usb::state().format == midi::Format::UsbMidi1);
    output.clear();
}

void set_alt(std::uint8_t alt, bool complete_pending = true)
{
    if (complete_pending)
    {
        drain();
    }
    tusb_control_request_t request{};
    request.bmRequestType = 1;
    request.bRequest = TUSB_REQ_SET_INTERFACE;
    request.wValue = alt;
    request.wIndex = 1;
    CHECK(midi2d_control_xfer_cb(0, CONTROL_STAGE_SETUP, &request) == (alt <= 1));
}

void receive_bytes(std::span<const std::uint8_t> bytes)
{
    while (!bytes.empty())
    {
        auto& out = transfers[1];
        CHECK(out.active);
        const auto size = std::min<std::size_t>(bytes.size(), out.size);
        std::memcpy(out.data, bytes.data(), size);
        out.active = false;
        CHECK(midi2d_xfer_cb(0, 1, XFER_RESULT_SUCCESS, size));
        bytes = bytes.subspan(size);
    }
}

void receive(std::span<const std::uint32_t> words)
{
    static_assert(std::endian::native == std::endian::little);
    receive_bytes({reinterpret_cast<const std::uint8_t*>(words.data()), words.size_bytes()});
}

std::vector<std::uint32_t> sent_words()
{
    drain();
    CHECK(output.size() % 4 == 0);
    std::vector<std::uint32_t> words(output.size() / 4);
    std::memcpy(words.data(), output.data(), output.size());
    output.clear();
    return words;
}

void submit(const midi::Message& message)
{
    const auto state = usb::state();
    CHECK(usb::write_message(message, state).error() == ESP_ERR_NOT_FINISHED);
    CHECK(usb::write_message(message, state).error() == ESP_ERR_NOT_FINISHED);
    tud_sof_cb(0);
    CHECK(usb::write_message(message, state));
}

void descriptor_tests()
{
    CHECK(config.descriptor.device->idVendor == 0x303A);
    CHECK(config.descriptor.device->idProduct == 0x4062);
    CHECK(config.descriptor.device->bNumConfigurations == 1);
    CHECK(std::strcmp(config.descriptor.string[2], "Qualia MIDI2") == 0);
    CHECK(std::strcmp(config.descriptor.string[3], "001122334455") == 0);
    const auto* descriptor = config.descriptor.full_speed_config;
    const auto length = static_cast<std::size_t>(descriptor[2] | descriptor[3] << 8);
    CHECK(length == TUD_CONFIG_DESC_LEN + TUD_MIDI2_DESC_LEN);
    CHECK(descriptor[4] == 2);
    unsigned interfaces = 0;
    unsigned endpoints = 0;
    for (std::size_t offset = 0; offset < length; offset += descriptor[offset])
    {
        CHECK(descriptor[offset] >= 2 && offset + descriptor[offset] <= length);
        if (descriptor[offset + 1] == TUSB_DESC_INTERFACE)
        {
            CHECK(descriptor[offset + 2] == (interfaces == 0 ? 0 : 1));
            CHECK(descriptor[offset + 3] == (interfaces == 2 ? 1 : 0));
            ++interfaces;
        }
        if (descriptor[offset + 1] == TUSB_DESC_ENDPOINT)
        {
            CHECK(descriptor[offset + 2] == (endpoints % 2 == 0 ? 1 : 0x81));
            CHECK(descriptor[offset + 4] == 64 && descriptor[offset + 5] == 0);
            ++endpoints;
        }
    }
    CHECK(interfaces == 3 && endpoints == 4);
    tusb_control_request_t request{};
    request.bmRequestType = 0x81;
    request.bRequest = TUSB_REQ_GET_DESCRIPTOR;
    request.wValue = 0x2601;
    request.wIndex = 1;
    request.wLength = 255;
    CHECK(midi2d_control_xfer_cb(0, CONTROL_STAGE_SETUP, &request));
    CHECK((control == std::vector<std::uint8_t>{5, 0x26, 1, 18, 0, 13, 0x26, 2, 1, 0, 0, 1, 4, 0x11, 0, 0, 0, 0}));
    request.wValue = 0x2600;
    CHECK(!midi2d_control_xfer_cb(0, CONTROL_STAGE_SETUP, &request));
    set_alt(2);
}

void io_tests()
{
    const auto legacy = midi::note_message({60, 0.5}, true, midi::Format::UsbMidi1);
    submit(legacy);
    drain();
    CHECK((output == std::vector<std::uint8_t>{9, 0x90, 60, 64}));
    output.clear();
    receive_bytes(legacy.packet());
    CHECK(**usb::read_message() == legacy);
    CHECK(!*usb::read_message());
    set_alt(1);
    CHECK(usb::state().format == midi::Format::UmpMidi2);
    const auto note = midi::note_message({60, 0.5}, true, midi::Format::UmpMidi2);
    submit(note);
    drain();
    CHECK((output == std::vector<std::uint8_t>{0, 60, 0x90, 0x40, 0, 0, 0, 0x80}));
    output.clear();
    CHECK(usb::write_message(legacy, usb::state()).error() == ESP_ERR_INVALID_ARG);
    CHECK(usb::write_message({{}, 0, true}, usb::state()).error() == ESP_ERR_INVALID_ARG);
    constexpr std::array<std::uint32_t, 10> input{0x20903C40, 0x30167D51, 0x55414C49, 0xB0000000, 1,
                                                  2,          0xD0000000, 3,          4,          5};
    receive(input);
    for (unsigned size : {1, 2, 3, 4})
    {
        const auto message = usb::read_message();
        CHECK(message && *message && (**message).size == size);
    }
    CHECK(!*usb::read_message());
    constexpr std::array<std::uint32_t, 3> partial{0xF0035155, 0x414C4941, 0};
    receive(partial);
    CHECK(!*usb::read_message());
    CHECK(transfers[1].active && transfers[1].size == 64);
    receive(std::array<std::uint32_t, 1>{0});
    CHECK(!*usb::read_message());
    constexpr std::array<std::uint32_t, 5> mixed{0x20903C40, 0xF0000101, 0x1F, 0, 0};
    receive(mixed);
    CHECK((**usb::read_message()).words[0] == mixed[0]);
    CHECK(!*usb::read_message());
    for (unsigned i = 0; i < 5; ++i)
    {
        tud_sof_cb(i);
        drain();
    }
    const auto replies = sent_words();
    CHECK(replies.size() == 20);
    CHECK(replies[0] == 0xF0060200);
    CHECK(replies[4] == 0xF0010101 && replies[5] == 0x81000300);
    CHECK(replies[8] == 0xF0020000 && replies[9] == 0x007D0000);
    receive(std::array<std::uint32_t, 4>{0xF010FF03, 0, 0, 0});
    tud_sof_cb(0);
    drain();
    tud_sof_cb(0);
    CHECK((sent_words() ==
           std::vector<std::uint32_t>{0xF0118033, 0x00010000, 0, 0, 0xF0120042, 0x6F756E63, 0x65000000, 0}));
}

void pressure_tests()
{
    const auto note = midi::note_message({62, 0.5}, true, midi::Format::UmpMidi2);
    CHECK(tud_midi2_ump_write(note.words.data(), note.size) == 2);
    for (unsigned i = 0; i < 8; ++i)
    {
        CHECK(tud_midi2_ump_write(note.words.data(), note.size) == 2);
    }
    CHECK(tud_midi2_ump_write(note.words.data(), note.size) == 0);
    const auto state = usb::state();
    CHECK(usb::write_message(note, state).error() == ESP_ERR_NOT_FINISHED);
    receive(std::array<std::uint32_t, 4>{0xF0000101, 1, 0, 0});
    tud_sof_cb(0);
    CHECK(usb::write_message(note, state).error() == ESP_ERR_TIMEOUT);
    drain();
    tud_sof_cb(0);
    CHECK(usb::write_message(note, state));
    const auto replies = sent_words();
    CHECK(replies.size() == 24);
    CHECK(replies[18] == 0xF0010101 && replies[22] == note.words[0]);

    CHECK(usb::write_message(note, state).error() == ESP_ERR_NOT_FINISHED);
    const auto off = midi::note_message({62, 0.5}, false, midi::Format::UmpMidi2);
    CHECK(usb::write_message(off, state).error() == ESP_ERR_NOT_FINISHED);
    tud_sof_cb(0);
    CHECK(usb::write_message(off, state));
    CHECK((sent_words() == std::vector<std::uint32_t>{off.words[0], 0}));
    CHECK(usb::write_message(note, state).error() == ESP_ERR_NOT_FINISHED);
    tud_sof_cb(0);
    CHECK(usb::write_message(off, state).error() == ESP_ERR_NOT_FINISHED);
    tud_sof_cb(0);
    CHECK(usb::write_message(off, state));
    CHECK((sent_words() == std::vector<std::uint32_t>{note.words[0], note.words[1], off.words[0], 0}));
    CHECK(usb::write_message(note, state).error() == ESP_ERR_NOT_FINISHED);
    during_send = [] {
        const auto off = midi::note_message({62, 0.5}, false, midi::Format::UmpMidi2);
        CHECK(usb::write_message(off, usb::state()).error() == ESP_ERR_NOT_FINISHED);
    };
    tud_sof_cb(0);
    CHECK(usb::write_message(note, state));
    CHECK((sent_words() == std::vector<std::uint32_t>{note.words[0], note.words[1]}));

    CHECK(tud_midi2_ump_write(note.words.data(), note.size) == 2);
    for (unsigned i = 0; i < 6; ++i)
    {
        CHECK(tud_midi2_ump_write(note.words.data(), note.size) == 2);
    }
    receive(std::array<std::uint32_t, 4>{0xF0000101, 1, 0, 0});
    CHECK(usb::write_message(off, state).error() == ESP_ERR_NOT_FINISHED);
    tud_sof_cb(0);
    CHECK(usb::write_message(off, state).error() == ESP_ERR_TIMEOUT);
    complete();
    for (unsigned i = 0; i < 6; ++i)
    {
        CHECK(tud_midi2_ump_write(note.words.data(), note.size) == 2);
    }
    receive(std::array<std::uint32_t, 4>{0xF0000101, 1, 0, 0});
    tud_sof_cb(0);
    CHECK(usb::write_message(off, state));
    drain();
    tud_sof_cb(0);
    drain();
    output.clear();
}

void session_tests()
{
    auto previous = usb::state();
    const auto note = midi::note_message({65, 0.5}, true, midi::Format::UmpMidi2);
    CHECK(usb::write_message(note, previous).error() == ESP_ERR_NOT_FINISHED);
    receive(std::array<std::uint32_t, 4>{0xF0050100, 0, 0, 0});
    CHECK(usb::state().format == midi::Format::UmpMidi1 && usb::state().generation != previous.generation);
    const auto ump1 = midi::note_message({65, 0.5}, false, midi::Format::UmpMidi1);
    CHECK(usb::write_message(ump1, usb::state()).error() == ESP_ERR_NOT_FINISHED);
    tud_sof_cb(0);
    CHECK(usb::write_message(ump1, usb::state()));
    CHECK((sent_words() == std::vector<std::uint32_t>{0xF0060100, 0, 0, 0, 0x20804100}));
    receive(std::array<std::uint32_t, 4>{0xF0057F03, 0, 0, 0});
    CHECK(usb::state().format == midi::Format::UmpMidi1);
    tud_sof_cb(0);
    CHECK((sent_words() == std::vector<std::uint32_t>{0xF0060100, 0, 0, 0}));
    const auto queued = midi::note_message({60, 0.5}, true, midi::Format::UmpMidi1);
    CHECK(tud_midi2_ump_write(queued.words.data(), queued.size) == 1);
    for (unsigned i = 0; i < 14; ++i)
    {
        CHECK(tud_midi2_ump_write(queued.words.data(), queued.size) == 1);
    }
    receive(std::array<std::uint32_t, 4>{0xF0050200, 0, 0, 0});
    CHECK(usb::write_message(note, usb::state()).error() == ESP_ERR_NOT_FINISHED);
    tud_sof_cb(0);
    CHECK(usb::write_message(note, usb::state()).error() == ESP_ERR_NOT_FINISHED);
    drain();
    tud_sof_cb(0);
    CHECK(usb::write_message(note, usb::state()));
    const auto saturated = sent_words();
    CHECK(saturated.size() == 21);
    CHECK(saturated[15] == 0xF0060200 && saturated[19] == note.words[0]);
    const auto partial_note = std::array<std::uint32_t, 1>{0x40904100};
    receive(partial_note);
    CHECK(tud_midi2_ump_write(note.words.data(), note.size) == note.size);
    CHECK(transfers[0x81].active);
    event(TINYUSB_EVENT_SUSPENDED);
    event(TINYUSB_EVENT_RESUMED);
    set_alt(1, false);
    CHECK(!transfers[0x81].active);
    CHECK(transfers[1].active && transfers[1].size == 64);
    CHECK(!*usb::read_message());
    receive(std::array<std::uint32_t, 4>{0xF0000101, 1, 0, 0});
    tud_sof_cb(0);
    CHECK((sent_words() == std::vector<std::uint32_t>{0xF0010101, 0x81000300, 0, 0}));
    submit(note);
    CHECK((sent_words() == std::vector<std::uint32_t>{note.words[0], note.words[1]}));
    set_alt(0);
    const auto legacy = midi::note_message({65, 0.5}, true, midi::Format::UsbMidi1);
    submit(legacy);
    CHECK(transfers[0x81].active);
    set_alt(0, false);
    CHECK(!transfers[0x81].active);
    CHECK(transfers[1].active && transfers[1].size == 64);
    submit(legacy);
    drain();
    CHECK((output == std::vector<std::uint8_t>{9, 0x90, 65, 64}));
    output.clear();
    set_alt(1);
    previous = usb::state();
    receive(std::array<std::uint32_t, 1>{0x20903C40});
    event(TINYUSB_EVENT_SUSPENDED);
    tud_sof_cb(0);
    CHECK(!usb::state().connected && usb::state().generation != previous.generation);
    CHECK(!*usb::read_message());
    event(TINYUSB_EVENT_RESUMED);
    CHECK(usb::state().connected && usb::state().format == midi::Format::UmpMidi2);
    CHECK(usb::write_message(ump1, previous).error() == ESP_ERR_INVALID_STATE);
    previous = usb::state();
    tud_event_hook_cb(0, DCD_EVENT_BUS_RESET, true);
    tud_sof_cb(0);
    CHECK(!usb::state().connected && usb::state().generation != previous.generation);
    connect();
    set_alt(1);
    previous = usb::state();
    std::array<std::uint32_t, 33> flood{};
    flood.fill(0x20903C40);
    receive(flood);
    CHECK(usb::read_message().error() == ESP_ERR_NO_MEM);
    CHECK(usb::state().generation != previous.generation);
    CHECK(!*usb::read_message());
    receive(std::array<std::uint32_t, 1>{0x20903E40});
    CHECK((**usb::read_message()).words[0] == 0x20903E40);
    receive(std::array<std::uint32_t, 1>{0x20903C40});
    set_alt(0);
    CHECK(!*usb::read_message() && usb::state().format == midi::Format::UsbMidi1);
    event(TINYUSB_EVENT_DETACHED);
    tud_sof_cb(0);
    CHECK(!usb::state().connected);
}

struct Transport
{
    static esp_err_t init()
    {
        return ESP_OK;
    }
    static midi::State state()
    {
        return usb::state();
    }
    static auto read_message()
    {
        return usb::read_message();
    }
    static auto write_message(const midi::Message& message, midi::State state)
    {
        return usb::write_message(message, state);
    }
};

void controller_tests()
{
    using namespace std::chrono_literals;
    for (auto format : {midi::Format::UsbMidi1, midi::Format::UmpMidi1, midi::Format::UmpMidi2})
    {
        connect();
        if (format != midi::Format::UsbMidi1)
        {
            set_alt(1);
            if (format == midi::Format::UmpMidi1)
            {
                receive(std::array<std::uint32_t, 4>{0xF0050100, 0, 0, 0});
                tud_sof_cb(0);
                drain();
                output.clear();
            }
        }
        modules::MidiController<Transport> controller;
        CHECK(controller.init() == ESP_OK);
        controller.step(0us);
        CHECK(controller.can_submit());
        CHECK(controller.submit({60, 0.5}) == ESP_OK);
        controller.step(1ms);
        tud_sof_cb(0);
        controller.step(2ms);
        controller.step(101ms);
        drain();
        CHECK(output.size() == (format == midi::Format::UmpMidi2 ? 8 : 4));
        controller.step(102ms);
        tud_sof_cb(0);
        controller.step(103ms);
        CHECK(controller.submit({62, 0.5}) == ESP_OK);
        controller.step(104ms);
        tud_sof_cb(0);
        controller.step(105ms);
        CHECK(controller.submit({62, 0.75}) == ESP_OK);
        controller.step(106ms);
        tud_sof_cb(0);
        controller.step(107ms);
        tud_sof_cb(0);
        controller.step(108ms);
        CHECK(controller.submit({64, 0.5}) == ESP_OK);
        controller.step(109ms);
        if (format == midi::Format::UsbMidi1)
        {
            constexpr std::array<std::uint8_t, 16> command{4, 0xF0, 0x7D, 0x51, 4, 0x55, 0x41, 0x4C,
                                                           4, 0x49, 0x41, 1,    5, 0xF7, 0,    0};
            receive_bytes(command);
        }
        else
        {
            receive(std::array<std::uint32_t, 4>{0x30167D51, 0x55414C49, 0x30324101, 0});
        }
        controller.step(110ms);
        CHECK(controller.bootloader_requested() && !controller.can_submit());
        for (unsigned time = 111; time <= 115; ++time)
        {
            controller.step(std::chrono::milliseconds(time));
            tud_sof_cb(0);
        }
        CHECK(controller.bootloader_ready());
        drain();
        std::vector<std::uint8_t> expected;
        const auto append = [&](midi::Note note, bool on) {
            const auto message = midi::note_message(note, on, format);
            const auto* bytes = reinterpret_cast<const std::uint8_t*>(message.words.data());
            expected.insert(expected.end(), bytes, bytes + message.size * 4);
        };
        append({60, 0.5}, true);
        append({60, 0.5}, false);
        append({62, 0.5}, true);
        append({62, 0.5}, false);
        append({62, 0.75}, true);
        append({62, 0.75}, false);
        append({64, 0.5}, false);
        CHECK(output == expected);
        output.clear();
    }
}
} // namespace

esp_err_t esp_read_mac(std::uint8_t* mac, int)
{
    constexpr std::array<std::uint8_t, 6> value{0, 0x11, 0x22, 0x33, 0x44, 0x55};
    std::memcpy(mac, value.data(), value.size());
    return ESP_OK;
}

esp_err_t tinyusb_driver_install(const tinyusb_config_t* value)
{
    config = *value;
    midi2d_init();
    return ESP_OK;
}

extern "C"
{
    bool usbd_edpt_open(std::uint8_t, const tusb_desc_endpoint_t*)
    {
        return true;
    }
    void usbd_edpt_close(std::uint8_t, std::uint8_t endpoint)
    {
        transfers[endpoint] = {};
    }
    bool usbd_edpt_claim(std::uint8_t, std::uint8_t endpoint)
    {
        if (transfers[endpoint].active)
        {
            return false;
        }
        transfers[endpoint].active = true;
        return true;
    }
    bool usbd_edpt_release(std::uint8_t, std::uint8_t endpoint)
    {
        transfers[endpoint].active = false;
        return true;
    }
    bool usbd_edpt_xfer(std::uint8_t, std::uint8_t endpoint, std::uint8_t* data, std::uint16_t size, bool)
    {
        transfers[endpoint] = {data, size, true};
        if (endpoint == 0x81 && during_send != nullptr)
        {
            const auto callback = during_send;
            during_send = nullptr;
            callback();
        }
        return true;
    }
    bool usbd_edpt_xfer_fifo(std::uint8_t, std::uint8_t, tu_fifo_t*, std::uint16_t, bool)
    {
        CHECK(false);
        return false;
    }
    bool tud_control_xfer(std::uint8_t, const tusb_control_request_t*, void* data, std::uint16_t size)
    {
        const auto* bytes = static_cast<const std::uint8_t*>(data);
        control.assign(bytes, bytes + size);
        return true;
    }
    bool tud_control_status(std::uint8_t, const tusb_control_request_t*)
    {
        return true;
    }
    void usbd_sof_enable(std::uint8_t, sof_consumer_t, bool)
    {
    }
    void tud_sof_cb_enable(bool)
    {
    }
    void usbd_spin_lock(bool)
    {
    }
    void usbd_spin_unlock(bool)
    {
    }
    bool tud_rhport_init(std::uint8_t, const tusb_rhport_init_t*)
    {
        return true;
    }
    bool tud_deinit(std::uint8_t)
    {
        return midi2d_deinit();
    }
    bool tud_inited()
    {
        return true;
    }
    void dcd_int_handler(std::uint8_t)
    {
    }
    std::uint32_t tusb_time_millis_api()
    {
        return 0;
    }
}

int main()
{
    CHECK(usb::read_message().error() == ESP_ERR_INVALID_STATE);
    CHECK(usb::init() == ESP_OK);
    CHECK(usb::init() == ESP_ERR_INVALID_STATE);
    connect();
    descriptor_tests();
    io_tests();
    pressure_tests();
    session_tests();
    controller_tests();
    CHECK(midi2d_deinit());
}
