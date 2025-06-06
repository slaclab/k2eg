#ifndef K2EG_COMMON_TIMEWINDOWEVENTBUFFER_H_
#define K2EG_COMMON_TIMEWINDOWEVENTBUFFER_H_

#include "k2eg/common/types.h"
#include <chrono>
#include <iostream>
#include <memory>
#include <vector>
#include <mutex>
#include <type_traits>

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

    std::vector<Entry> buffer;
    size_t             head = 0; // points to the oldest element
    size_t             tail = 0; // points to the next insertion point
    size_t             count = 0;
    std::mutex         buffer_mutex;
    std::chrono::milliseconds window_ms;
    size_t            max_size;
    size_t            grow_factor;

public:
    explicit TimeWindowEventBuffer(size_t window = 1000, size_t reserve = 1024, size_t grow = 2)
        : buffer(reserve), window_ms(std::chrono::milliseconds(window)), max_size(reserve), grow_factor(grow)
    {}

    void setTimeWindow(std::chrono::milliseconds window)
    {
        window_ms = window;
    }

    bool push(std::shared_ptr<T> value, TimePoint timestamp)
    {
        pruneOldIfNeeded(timestamp);

        if (count == buffer.size()) {
            expandBuffer();
        }

        buffer[tail] = Entry{timestamp, std::move(value)};
        tail = (tail + 1) % buffer.size();
        if (count < buffer.size()) {
            ++count;
        } else {
            // Overwrite oldest
            head = (head + 1) % buffer.size();
        }
        return true;
    }

    std::vector<std::shared_ptr<T>> fetchWindow()
    {
        std::vector<std::shared_ptr<T>> out;
        for (size_t i = 0, idx = head; i < count; ++i, idx = (idx + 1) % buffer.size()) {
            out.push_back(buffer[idx].event);
        }
        return out;
    }

    template <typename OutT, typename Func>
    std::vector<OutT> fetchWindow(Func&& filter_map)
    {
        std::vector<OutT> out;
        for (size_t i = 0, idx = head; i < count; ++i, idx = (idx + 1) % buffer.size()) {
            auto maybe = filter_map(buffer[idx].event);
            if constexpr (std::is_same_v<OutT, decltype(maybe)>) {
                out.push_back(maybe);
            } else {
                if (maybe)
                    out.push_back(*maybe);
            }
        }
        return out;
    }

    void reset()
    {
        buffer.clear();
        buffer.resize(max_size);
        head = tail = count = 0;
    }

private:
    void pruneOldIfNeeded(TimePoint now)
    {
        // Remove old entries from the head
        while (count > 0 && now - buffer[head].timestamp > window_ms) {
            head = (head + 1) % buffer.size();
            --count;
        }
    }

    void expandBuffer()
    {
        size_t new_size = buffer.size() == 0 ? max_size : buffer.size() * grow_factor;
        std::vector<Entry> new_buffer(new_size);

        // Copy elements in order
        for (size_t i = 0; i < count; ++i) {
            new_buffer[i] = buffer[(head + i) % buffer.size()];
        }
        buffer = std::move(new_buffer);
        head = 0;
        tail = count;
    }
};
} // namespace k2eg::common

#endif // K2EG_COMMON_TIMEWINDOWEVENTBUFFER_H_