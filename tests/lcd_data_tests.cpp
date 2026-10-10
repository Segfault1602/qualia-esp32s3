#include "lcd_config.h"
#include "lcd_init_commands.h"

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

int main()
{
    static_assert(drivers::lcd::WIDTH == 480 && drivers::lcd::HEIGHT == 480);
    static_assert(drivers::lcd::FRAMEBUFFER_BYTES == 460'800);
    static_assert(drivers::lcd::INIT_COMMANDS.size() == 38);
    std::uint64_t hash = 14695981039346656037ULL;
    const auto append = [&hash](std::uint8_t byte) { hash = (hash ^ byte) * 1099511628211ULL; };
    for (const auto& command : drivers::lcd::INIT_COMMANDS)
    {
        CHECK(command.command >= 0 && command.command <= 255);
        CHECK(command.data.size() <= 16);
        append(static_cast<std::uint8_t>(command.command));
        append(static_cast<std::uint8_t>(command.data.size()));
        for (unsigned shift = 0; shift < 32; shift += 8)
        {
            append(static_cast<std::uint8_t>(command.delay_ms >> shift));
        }
        for (const auto byte : command.data)
        {
            append(byte);
        }
    }
    CHECK(hash == 0xd752db7ada54f1cbULL);
    CHECK(drivers::lcd::INIT_COMMANDS[36].data.empty());
    CHECK(drivers::lcd::INIT_COMMANDS[36].delay_ms == 120);
    CHECK(drivers::lcd::INIT_COMMANDS[37].data.empty());
    return EXIT_SUCCESS;
}
