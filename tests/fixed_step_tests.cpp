#include "fixed_step.h"

#include <cstdio>
#include <cstdlib>

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    std::exit(EXIT_FAILURE); \
} } while (false)

int main()
{
    {
        physics::FixedStepClock clock(60, 4);
        CHECK(clock.advance(0) == 0);
        CHECK(clock.advance(16'666) == 0);
        CHECK(clock.pending_us() == 16'666);
        CHECK(clock.advance(1) == 1);
        CHECK(clock.pending_us() == 0);
    }
    {
        physics::FixedStepClock clock(60, 4);
        std::uint32_t steps = 0;
        for (int i = 0; i < 1000; ++i)
        {
            steps += clock.advance(1'000);
        }
        CHECK(steps == 60);
        CHECK(clock.pending_us() == 0);
    }
    {
        physics::FixedStepClock clock(60, 4);
        std::uint32_t steps = 0;
        for (int i = 0; i < 60; ++i)
        {
            steps += clock.advance(18'000);
        }
        CHECK(steps == 64);
        CHECK(clock.pending_us() == 13'333);
    }
    {
        physics::FixedStepClock clock(60, 4);
        CHECK(clock.advance(100'000) == 4);
        CHECK(clock.pending_us() == 0);
        CHECK(clock.advance(10'000) == 0);
        CHECK(clock.advance(1'000'000'000) == 4);
        CHECK(clock.pending_us() < 16'667);
    }
    {
        physics::FixedStepClock clock(60, 4);
        CHECK(clock.advance(4 * 16'667) == 4);
        CHECK(clock.advance(16'667) == 1);
    }
    return EXIT_SUCCESS;
}
