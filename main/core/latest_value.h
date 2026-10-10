#pragma once

#include <mutex>
#include <optional>

/**
 * @brief Single-slot value handed from one task to another; readers always see the newest complete value.
 */
template <typename T>
class LatestValue
{
  public:
    void publish(const T& value)
    {
        std::lock_guard lock(mutex_);
        value_ = value;
    }

    std::optional<T> load() const
    {
        std::lock_guard lock(mutex_);
        return value_;
    }

  private:
    mutable std::mutex mutex_;
    std::optional<T> value_;
};
