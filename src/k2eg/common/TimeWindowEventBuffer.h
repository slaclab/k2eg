#ifndef K2EG_COMMON_TIMEWINDOWEVENTBUFFER_H_
#define K2EG_COMMON_TIMEWINDOWEVENTBUFFER_H_

#include <atomic>
#include <chrono>
#include <memory>
#include <vector>

namespace k2eg::common {

template <typename T>
class TimeWindowEventBuffer
{
    using TimePoint = std::chrono::steady_clock::time_point;

    struct Entry
    {
        TimePoint          timestamp;
        std::shared_ptr<T> event;
    };

    std::vector<Entry>        buffer;
    std::mutex                buffer_mutex; // <-- Add this
    std::atomic<bool>         accepting_data{true};
    std::chrono::milliseconds window_ms;
    size_t                    start_idx = 0;
    size_t                    max_size;    // Initial reserve and step if zero
    size_t                    grow_factor; // Multiplicative growth

public:
    explicit TimeWindowEventBuffer(std::chrono::milliseconds window = std::chrono::milliseconds(1000), size_t reserve = 1024, size_t grow = 2)
        : window_ms(window), max_size(reserve), grow_factor(grow)
    {
        buffer.reserve(max_size);
    }

    void setDataTakingEnabled(bool enable)
    {
        accepting_data.store(enable, std::memory_order_release);
    }

    void setTimeWindow(std::chrono::milliseconds window)
    {
        window_ms = window;
    }

    bool push(std::shared_ptr<T> value, TimePoint timestamp)
    {
        if (!accepting_data.load(std::memory_order_acquire))
            return false;
        std::lock_guard<std::mutex> lock(buffer_mutex); // <-- Lock here
        pruneOldIfNeeded(timestamp);
        if (buffer.size() == buffer.capacity())
        {
            size_t new_cap = buffer.capacity() == 0 ? max_size : buffer.capacity() * grow_factor;
            buffer.reserve(new_cap);
        }
        buffer.push_back(Entry{timestamp, std::move(value)});
        return true;
    }

    std::vector<std::shared_ptr<T>> fetchWindow()
    {
        std::lock_guard<std::mutex>     lock(buffer_mutex); // <-- Lock for read safety
        std::vector<std::shared_ptr<T>> out;
        for (size_t i = start_idx; i < buffer.size(); ++i)
        {
            out.push_back(buffer[i].event);
        }
        return out;
    }

    void reset()
    {
        // Lock reset
        std::lock_guard<std::mutex> lock(buffer_mutex);
        // don't shrink, keep allocation for speed
        buffer.clear();
        start_idx = 0;
        accepting_data.store(true, std::memory_order_release);
    }

private:
    void pruneOldIfNeeded(TimePoint now)
    {
        // Assumes buffer_mutex is held!
        while (start_idx < buffer.size() && now - buffer[start_idx].timestamp > window_ms)
            ++start_idx;
        // Compact only if a lot of unused space
        if (start_idx > buffer.size() / 2)
        {
            buffer.erase(buffer.begin(), buffer.begin() + start_idx);
            start_idx = 0;
        }
    }
};

} // namespace k2eg::common

#endif // K2EG_COMMON_TIMEWINDOWEVENTBUFFER_H_