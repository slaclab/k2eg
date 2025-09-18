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
        : stats{0, 0, 0, min_throttle_us}
        , last_update_time_(std::chrono::steady_clock::now()) {}

    ThrottlingManager(int min_us, int max_us, int threshold) noexcept
        : stats{0, 0, 0, min_us}
        , min_throttle_us_(min_us)
        , max_throttle_us_(max_us)
        , idle_threshold_(threshold)
        , last_update_time_(std::chrono::steady_clock::now()) {}

    // Backward-compatible API: treat true as 1 event, false as 0
    void update(bool had_events) { update(had_events ? 1 : 0); }

    // Adaptive update that considers the number of events processed in this cycle
    // Sleep-based adaptive pacing that considers the number of events processed
    void update(int events_count)
    {
        // Normalize negative inputs
        if (events_count < 0) events_count = 0;

        // Calculate elapsed time since last update and instantaneous EPS
        auto now = std::chrono::steady_clock::now();
        double dt_s = std::chrono::duration<double>(now - last_update_time_).count();
        if (dt_s <= 0.0) dt_s = 1e-6; // guard against zero or negative deltas
        last_update_time_ = now;

        const double inst_eps = static_cast<double>(events_count) / dt_s;
        // Exponential moving average of EPS for stability
        ema_eps_ = ema_alpha_ * inst_eps + (1.0 - ema_alpha_) * ema_eps_;

        if (events_count == 0 && ema_eps_ < eps_idle_threshold_)
        {
            // Completely idle cycle: ramp up throttle periodically
            stats.idle_counter++;
            stats.total_idle_cycles++;
            if (stats.idle_counter >= idle_threshold_)
            {
                stats.throttle_us = std::min(stats.throttle_us * 2, max_throttle_us_);
                stats.idle_counter = 0;
            }
            // Sleep based on current throttle when idle
            if (stats.throttle_us > 0)
            {
                std::this_thread::sleep_for(std::chrono::microseconds(stats.throttle_us));
            }
            return;
        }

        // Some activity detected (or recent activity per EMA)
        stats.total_events_processed.fetch_add(events_count, std::memory_order_relaxed);
        // Compute desired pacing from EMA of events/second: aim to process ~target_events_per_poll_ per poll
        const double eps = std::max(ema_eps_, eps_floor_);
        const double desired_delay_us_d = 1'000'000.0 * (target_events_per_poll_ / eps);
        const int    desired_delay_us = static_cast<int>(std::clamp(desired_delay_us_d, static_cast<double>(min_throttle_us_), static_cast<double>(max_throttle_us_)));

        stats.throttle_us = desired_delay_us;
        stats.idle_counter = 0;
        if (desired_delay_us > 0)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(desired_delay_us));
        }
        return;
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
    // Event rate tracking
    std::chrono::steady_clock::time_point last_update_time_{};
    double ema_eps_ = 0.0;           // exponential moving average of events per second
    double ema_alpha_ = 0.2;         // smoothing factor for EMA
    double target_events_per_poll_ = 8.0; // target events handled per poll
    double eps_floor_ = 0.1;         // minimum EPS to avoid division by zero
    double eps_idle_threshold_ = 0.25; // below this EMA EPS, treat as idle when no events in the current cycle
};

DEFINE_PTR_TYPES(ThrottlingManager)
} // namespace k2eg::common

#endif // K2EG_COMMON_THROTTLEMANAGEMENT_H_
