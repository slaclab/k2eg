#ifndef K2EG_COMMON_LOCKFREESNAPSHOTBUFFER_H_
#define K2EG_COMMON_LOCKFREESNAPSHOTBUFFER_H_

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace k2eg::common {

template <typename T>
class LockFreeBuffer
{
    std::vector<std::shared_ptr<T>> buffer;
    std::atomic<size_t>             write_idx{0};
    std::atomic<bool>               accepting_data{true};
    std::mutex                      resize_mtx;
    const size_t                    initial_capacity;
    const size_t                    growth_factor; // e.g., 2 for doubling

public:
    explicit LockFreeBuffer(size_t reserve = 1024, size_t growth = 2)
        : buffer(reserve), initial_capacity(reserve), growth_factor(growth)
    {
    }

    bool push(std::shared_ptr<T> value)
    {
        if (!accepting_data.load(std::memory_order_acquire))
            return false;
        size_t idx = write_idx.fetch_add(1, std::memory_order_acq_rel);

        if (idx >= buffer.size())
        {
            std::lock_guard<std::mutex> lock(resize_mtx);
            if (idx >= buffer.size())
            {
                size_t new_size = buffer.size() * growth_factor;
                if (new_size <= idx)
                {
                    new_size = idx + initial_capacity;
                }
                buffer.resize(new_size);
            }
            // Write to the slot while holding the lock to be sure
            buffer[idx] = std::move(value);
        }
        else
        {
            // No resize, so it's safe (as long as the buffer isn't resized concurrently)
            buffer[idx] = std::move(value);
        }
        return true;
    }

    void setDataTakingEnabled(bool enable)
    {
        accepting_data.store(enable, std::memory_order_release);
    }

    void reset()
    {
        write_idx.store(0, std::memory_order_release);
        accepting_data.store(true, std::memory_order_release);
        std::lock_guard<std::mutex> lock(resize_mtx);
        buffer.clear();
        buffer.resize(initial_capacity);
    }

    std::vector<std::shared_ptr<T>> fetchAll() const
    {
        size_t                          n = write_idx.load(std::memory_order_acquire);
        std::vector<std::shared_ptr<T>> out;
        out.reserve(n);
        for (size_t i = 0; i < n && i < buffer.size(); ++i)
        {
            out.push_back(buffer[i]);
        }
        return out;
    }
};

} // namespace k2eg::common

#endif // K2EG_COMMON_LOCKFREESNAPSHOTBUFFER_H_