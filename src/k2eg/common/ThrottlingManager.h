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
    int total_events_processed;
    int throttle_ms;
};

class ThrottlingManager
{
public:
    constexpr static int min_throttle_ms = 1;
    constexpr static int max_throttle_ms = 100;
    constexpr static int idle_threshold = 10;

    ThrottlingManager() noexcept
        : stats{0, 0, 0, min_throttle_ms} {}

    ThrottlingManager(int min_ms, int max_ms, int threshold) noexcept
        : stats{0, 0, 0, min_ms}, min_throttle_ms_(min_ms), max_throttle_ms_(max_ms), idle_threshold_(threshold) {}

    void update(bool had_events)
    {
        if (!had_events)
        {
            stats.idle_counter++;
            stats.total_idle_cycles++;
            if (stats.idle_counter >= idle_threshold_)
            {
                stats.throttle_ms = std::min(stats.throttle_ms * 2, max_throttle_ms_);
                std::this_thread::sleep_for(std::chrono::milliseconds(stats.throttle_ms));
                stats.idle_counter = 0;
            }
        }
        else
        {
            stats.idle_counter = 0;
            stats.total_events_processed++;
            stats.throttle_ms = std::max(stats.throttle_ms / 2, min_throttle_ms_);
        }
    }

    ThrottlingStats getStats() const noexcept { return stats; }

    void reset() noexcept
    {
        stats = {0, 0, 0, min_throttle_ms_};
    }

    ThrottlingManager(const ThrottlingManager&) = delete;
    ThrottlingManager& operator=(const ThrottlingManager&) = delete;

private:
    ThrottlingStats stats;
    int min_throttle_ms_ = min_throttle_ms;
    int max_throttle_ms_ = max_throttle_ms;
    int idle_threshold_ = idle_threshold;
};

DEFINE_PTR_TYPES(ThrottlingManager)
} // namespace k2eg::common

#endif // K2EG_COMMON_THROTTLEMANAGEMENT_H_