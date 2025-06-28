#ifndef K2EG_COMMON_THROTTLEMANAGEMENT_H_
#define K2EG_COMMON_THROTTLEMANAGEMENT_H_

#include <k2eg/common/types.h>

#include <chrono>
#include <thread>
#include <algorithm>

namespace k2eg::common {

struct ThrottlingStats {
    int idle_counter;
    int total_idle_cycles;
    std::atomic<int> total_events_processed;
    int throttle_us;
    ThrottlingStats(int idle = 0, int idle_cycles = 0, int events = 0, int throttle = 1)
        : idle_counter(idle), total_idle_cycles(idle_cycles), total_events_processed(events), throttle_us(throttle) {}

    // Custom copy constructor
    ThrottlingStats(const ThrottlingStats& other)
        : idle_counter(other.idle_counter),
          total_idle_cycles(other.total_idle_cycles),
          total_events_processed(other.total_events_processed.load(std::memory_order_relaxed)),
          throttle_us(other.throttle_us) {}

    //reset operator
    ThrottlingStats& operator=(const ThrottlingStats& other)
    {
        if (this != &other)
        {
            idle_counter = other.idle_counter;
            total_idle_cycles = other.total_idle_cycles;
            total_events_processed.store(other.total_events_processed.load(std::memory_order_relaxed), std::memory_order_relaxed);
            throttle_us = other.throttle_us;
        }
        return *this;
    }
};

class ThrottlingManager
{
public:
    constexpr static int min_throttle_us = 500;
    constexpr static int max_throttle_us = 100000;
    constexpr static int idle_threshold = 10;

    ThrottlingManager() noexcept
        : stats{0, 0, 0, min_throttle_us} {}

    ThrottlingManager(int min_us, int max_us, int threshold) noexcept
        : stats{0, 0, 0, min_us}, min_throttle_us_(min_us), max_throttle_us_(max_us), idle_threshold_(threshold) {}

    void update(bool had_events)
    {
        if (!had_events)
        {
            stats.idle_counter++;
            stats.total_idle_cycles++;
            if (stats.idle_counter >= idle_threshold_)
            {
                stats.throttle_us = std::min(stats.throttle_us * 2, max_throttle_us_);
                std::this_thread::sleep_for(std::chrono::microseconds(stats.throttle_us));
                stats.idle_counter = 0;
            }
        }
        else
        {
            stats.idle_counter = 0;
            stats.total_events_processed++;
            stats.throttle_us = min_throttle_us_;
        }
    }

    ThrottlingStats getStats() const noexcept { 
        // this work cause of the copy contructur
        return stats;
     }

    void reset() noexcept
    {
        stats = {0, 0, 0, max_throttle_us_};
    }

    void resetEventCounter() noexcept
    {
        stats.total_events_processed = 0;
    }

    ThrottlingManager(const ThrottlingManager&) = delete;
    ThrottlingManager& operator=(const ThrottlingManager&) = delete;

private:
    ThrottlingStats stats;
    int min_throttle_us_ = min_throttle_us;
    int max_throttle_us_ = max_throttle_us;
    int idle_threshold_ = idle_threshold;
};

DEFINE_PTR_TYPES(ThrottlingManager)
} // namespace k2eg::common

#endif // K2EG_COMMON_THROTTLEMANAGEMENT_H_