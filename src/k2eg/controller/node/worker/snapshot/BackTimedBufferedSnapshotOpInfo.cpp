#include <chrono>
#include <iostream>
#include <k2eg/controller/node/worker/snapshot/BackTimedBufferedSnapshotOpInfo.h>
#include <k2eg/controller/node/worker/snapshot/SnapshotOpInfo.h>
#include <k2eg/service/epics/EpicsData.h>

using namespace k2eg::controller::node::worker::snapshot;
using namespace k2eg::controller::command::cmd;
using namespace k2eg::service::epics_impl;
using namespace std::chrono;

BackTimedBufferedSnapshotOpInfo::BackTimedBufferedSnapshotOpInfo(const std::string& queue_name, ConstRepeatingSnapshotCommandShrdPtr cmd)
    : SnapshotOpInfo(queue_name, cmd), acquiring_buffer(MakeMonitorEventBacktimeBufferShrdPtr(cmd->time_window_msec)), processing_buffer(MakeMonitorEventBacktimeBufferShrdPtr(cmd->time_window_msec))
{
    if (cmd->sub_push_delay_msec > 0)
    {
        if (cmd->sub_push_delay_msec < cmd->time_window_msec)
        {
            fast_expire_time_msec = cmd->sub_push_delay_msec;
        }
        else
        {
            // If sub_push_delay_msec is not less than time_window_msec, use time_window_msec or a default
            fast_expire_time_msec = (cmd->time_window_msec > 0) ? cmd->time_window_msec : cmd->sub_push_delay_msec;
        }
    }
    else
    {
        // If sub_push_delay_msec is not set, check if the time window is greate then the default vclaue of sub push
        // delay
        if (cmd->time_window_msec > FAST_EXPIRE_TIME_MSEC)
        {
            fast_expire_time_msec = FAST_EXPIRE_TIME_MSEC;
        }
        else
        {
            // Use the default fast expire time
            fast_expire_time_msec = cmd->time_window_msec;
        }
    }
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

bool BackTimedBufferedSnapshotOpInfo::isTimeout(const std::chrono::steady_clock::time_point& now)
{
    bool timeout = false;
    win_time_expired = false;
    if (is_triggered)
    {
        // In triggered mode, only swap buffers on real timeout (trigger or stop).
        timeout = win_time_expired = SnapshotOpInfo::isTimeout(now);
        if (timeout)
        {
            // On timeout, swap the buffers so getData works on a stable snapshot.
            std::unique_lock lock(buffer_mutex);
            std::swap(acquiring_buffer, processing_buffer);
            last_forced_expire = now;
        }
    }
    else
    {
        // In timed mode, swap buffers on real timeout or every 100ms for fast processing.
        bool expired = SnapshotOpInfo::isTimeout(now);
        bool force_fast_expire = false;

        if (!expired && std::chrono::duration_cast<std::chrono::milliseconds>(now - last_forced_expire).count() >= fast_expire_time_msec)
        {
            force_fast_expire = true;
        }

        // Memorize the real window expiration
        win_time_expired = expired;

        if (expired || force_fast_expire)
        {
            timeout = true;
            last_forced_expire = now;
            std::unique_lock lock(buffer_mutex);
            std::swap(acquiring_buffer, processing_buffer);
        }
    }
    return timeout;
}

void BackTimedBufferedSnapshotOpInfo::addData(MonitorEventShrdPtr event_data)
{
    // Always add new data to the acquiring buffer.
    std::unique_lock lock(buffer_mutex);
    acquiring_buffer->push(event_data, steady_clock::now());
}

SnapshotSubmissionShrdPtr BackTimedBufferedSnapshotOpInfo::getData()
{
    SnapshotSubmissionType                                      type = SnapshotSubmissionType::None;
    std::vector<k2eg::service::epics_impl::MonitorEventShrdPtr> result;
    std::chrono::steady_clock::time_point submission_timestamp = last_forced_expire;
    {
        std::shared_lock lock(buffer_mutex);

        // Emit header only once at the start of the window
        if (!header_sent)
        {
            type |= SnapshotSubmissionType::Header;
            header_sent = true;
            header_snapshot_time = last_forced_expire;
        }

        // Emit data every 100ms if available

        if (processing_buffer)
        {
            if (cmd->pv_field_filter_list.size() > 0)
            {
                // If field filtering is needed, apply it to each event.
                result = processing_buffer->fetchWindow<MonitorEventShrdPtr>(
                    [this](auto ev) -> std::optional<MonitorEventShrdPtr>
                    {
                        return std::make_optional(MakeMonitorEventShrdPtr(
                            ev->type, ev->message,
                            ChannelData{ev->channel_data.pv_name, filterPVField(ev->channel_data.data, this->cmd->pv_field_filter_list)}));
                    });
            }
            else
            {
                // Fetch all events without filtering
                result = processing_buffer->fetchWindow();
            }
            if (!result.empty())
            {
                type |= SnapshotSubmissionType::Data;
            }
            // Clear buffer only if data is returned
            processing_buffer->reset();
        }

        // Emit tail only at the end of the window (timeout)
        // or as last messag ewhen the snapshot is killed.
        if (win_time_expired || !is_running)
        {
            // This means isTimeout() was just called and window swapped
            type |= SnapshotSubmissionType::Tail;
            header_sent = false; // Reset header for the next window
        }
    }
    // Iteration id is assigned by the scheduler; initialize as 0 here.
    return MakeSnapshotSubmissionShrdPtr(std::chrono::steady_clock::now(), std::move(result), type, 0);
}
