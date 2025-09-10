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
    : SnapshotOpInfo(queue_name, cmd), acquiring_buffer(MakeMonitoEventBacktimeBufferUPtr(cmd->time_window_msec)), processing_buffer(MakeMonitoEventBacktimeBufferUPtr(cmd->time_window_msec))
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
    // Initialize PV tracking sets
    std::unique_lock lock(buffer_mutex);
    all_pvs_.clear();
    pvs_no_events_.clear();
    events_per_pv_.clear();
    for (const auto& pv : sanitized_pv_name_list)
    {
        all_pvs_.insert(pv->name);
    }
    pvs_no_events_ = all_pvs_;
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
            if(expired){onWindowTimeout(true);}
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

    // Track per-PV statistics for fast lookup of silent PVs
    const auto& pv_name = event_data->channel_data.pv_name;
    pvs_no_events_.erase(pv_name);
    auto it = events_per_pv_.find(pv_name);
    if (it == events_per_pv_.end())
    {
        events_per_pv_.emplace(pv_name, 1);
    }
    else
    {
        ++(it->second);
    }
}

SnapshotSubmissionShrdPtr BackTimedBufferedSnapshotOpInfo::getData()
{
    SnapshotSubmissionType                                      type = SnapshotSubmissionType::None;
    std::vector<k2eg::service::epics_impl::MonitorEventShrdPtr> result;
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

void BackTimedBufferedSnapshotOpInfo::onWindowTimeout(bool /*full_window*/)
{
    // Reset per-window PV stats: mark all PVs as having no events, clear counters
    pvs_no_events_ = all_pvs_;
    events_per_pv_.clear();
}

std::vector<std::string> BackTimedBufferedSnapshotOpInfo::getPVsWithoutEvents() const
{
    std::shared_lock lock(buffer_mutex);
    std::vector<std::string> res;
    res.reserve(pvs_no_events_.size());
    for (const auto& pv : pvs_no_events_)
    {
        res.emplace_back(pv);
    }
    return res;
}
