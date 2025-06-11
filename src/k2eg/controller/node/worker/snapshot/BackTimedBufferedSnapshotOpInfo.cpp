#include <iostream>
#include <k2eg/controller/node/worker/snapshot/BackTimedBufferedSnapshotOpInfo.h>
#include <k2eg/controller/node/worker/snapshot/SnapshotOpInfo.h>
#include <k2eg/service/epics/EpicsData.h>

using namespace k2eg::controller::node::worker::snapshot;
using namespace k2eg::controller::command::cmd;
using namespace k2eg::service::epics_impl;
using namespace std::chrono;

#define FAST_EXPIRE_TIME_MSEC 100

BackTimedBufferedSnapshotOpInfo::BackTimedBufferedSnapshotOpInfo(const std::string& queue_name, ConstRepeatingSnapshotCommandShrdPtr cmd)
    : SnapshotOpInfo(queue_name, cmd), acquiring_buffer(MakeMonitoEventBacktimeBufferUPtr(cmd->time_window_msec)), processing_buffer(MakeMonitoEventBacktimeBufferUPtr(cmd->time_window_msec))
{
}

BackTimedBufferedSnapshotOpInfo::~BackTimedBufferedSnapshotOpInfo()
{
    acquiring_buffer.reset();
    processing_buffer.reset();
}

bool BackTimedBufferedSnapshotOpInfo::init(std::vector<PVShrdPtr>& sanitized_pv_name_list)
{
    // No initialization needed for PVs in this implementation.
    return true;
}

bool BackTimedBufferedSnapshotOpInfo::isTimeout()
{
    bool timeout = false;
    if (is_triggered)
    {
        // In triggered mode, only swap buffers on real timeout (trigger or stop).
        timeout = win_time_expired = SnapshotOpInfo::isTimeout();
        if (timeout)
        {
            // On timeout, swap the buffers so getData works on a stable snapshot.
            std::unique_lock<std::shared_mutex> lock(buffer_mutex);
            std::swap(acquiring_buffer, processing_buffer);
        }
    }
    else
    {
        // In timed mode, swap buffers on real timeout or every 100ms for fast processing.
        auto now = steady_clock::now();
        bool expired = SnapshotOpInfo::isTimeout();
        bool force_100ms_expire = false;

        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_forced_expire).count() >= FAST_EXPIRE_TIME_MSEC)
        {
            force_100ms_expire = true;
            last_forced_expire = now;
        }

        // Memorize the real window expiration
        win_time_expired = expired;

        if (expired || force_100ms_expire)
        {
            last_timeout_check = now;
            std::unique_lock<std::shared_mutex> lock(buffer_mutex);
            std::swap(acquiring_buffer, processing_buffer);
            timeout = true;
        }
    }
    return timeout;
}

void BackTimedBufferedSnapshotOpInfo::addData(MonitorEventShrdPtr event_data)
{
    // Always add new data to the acquiring buffer.
    std::shared_lock<std::shared_mutex> lock(buffer_mutex);
    acquiring_buffer->push(event_data, steady_clock::now());
}

SnapshotSubmission BackTimedBufferedSnapshotOpInfo::getData()
{
    SnapshotSubmissionType                                      type = SnapshotSubmissionType::None;
    std::vector<k2eg::service::epics_impl::MonitorEventShrdPtr> result;

    auto now = steady_clock::now();
    {
        std::shared_lock lock(buffer_mutex);

        // Emit header only once at the start of the window
        if (!header_sent)
        {
            type |= SnapshotSubmissionType::Header;
            header_sent = true;
        }

        // Emit data every 100ms if available

        if (processing_buffer)
        {
            result = processing_buffer->fetchWindow();
            if (!result.empty())
            {
                type |= SnapshotSubmissionType::Data;
            }
            // Clear buffer only if data is returned
            processing_buffer->reset();
        }

        // Emit tail only at the end of the window (timeout)
        // If tail_sent is false and the processing buffer is empty (after timeout), emit tail
        if (win_time_expired)
        {
            // This means isTimeout() was just called and window swapped
            type |= SnapshotSubmissionType::Tail;
            header_sent = false; // Reset header for the next window
        }
    }
    std::cout << "BackTimedBufferedSnapshotOpInfo::getData() - type: " << static_cast<int>(type) << ", result size: " << result.size() << std::endl;
    return SnapshotSubmission(std::move(result), type);
}