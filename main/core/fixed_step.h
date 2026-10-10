#pragma once

#include <cstdint>

namespace physics
{
// Converts elapsed wall time into whole fixed-rate steps, carrying the remainder.
// When more than max_steps are due, the backlog is dropped so slow frames
// cannot snowball into ever longer catch-up work.
class FixedStepClock
{
public:
    constexpr FixedStepClock(std::uint32_t steps_per_second, std::uint32_t max_steps)
        : steps_per_second_(steps_per_second), max_steps_(max_steps)
    {
    }

    constexpr std::uint32_t advance(std::uint64_t elapsed_us)
    {
        accumulator_ += elapsed_us * steps_per_second_;
        const std::uint64_t due = accumulator_ / MICROSECONDS_PER_SECOND;
        accumulator_ %= MICROSECONDS_PER_SECOND;
        if (due > max_steps_)
        {
            return max_steps_;
        }
        return static_cast<std::uint32_t>(due);
    }

    constexpr std::uint64_t pending_us() const
    {
        return accumulator_ / steps_per_second_;
    }

private:
    static constexpr std::uint64_t MICROSECONDS_PER_SECOND = 1'000'000;

    std::uint32_t steps_per_second_;
    std::uint32_t max_steps_;
    std::uint64_t accumulator_ = 0;
};
}
