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
}

bool BackTimedBufferedSnapshotOpInfo::init(std::vector<PVShrdPtr>& sanitized_pv_name_list)
{
    // No initialization needed for PVs in this implementation.
    return true;
}

bool BackTimedBufferedSnapshotOpInfo::isTimeout()
{
    auto is_timeout = SnapshotOpInfo::isTimeout();
    if (is_timeout)
    {
        // On timeout, swap the buffers so getData works on a stable snapshot.
        std::unique_lock<std::shared_mutex> lock(buffer_mutex);
        std::swap(acquiring_buffer, processing_buffer);
    }
    return is_timeout;
}

void BackTimedBufferedSnapshotOpInfo::addData(MonitorEventShrdPtr event_data)
{
    // Always add new data to the acquiring buffer.
    std::shared_lock<std::shared_mutex> lock(buffer_mutex);
    acquiring_buffer->push(event_data, steady_clock::now());
}

std::vector<MonitorEventShrdPtr> BackTimedBufferedSnapshotOpInfo::getData()
{
    // Fetch data from the processing buffer only; buffers are circular and self-pruning.
    std::shared_lock<std::shared_mutex> lock(buffer_mutex);
    if (this->cmd->pv_field_filter_list.size() == 0)
    {
        // No field filtering needed, return all.
        return processing_buffer->fetchWindow();
    }
    else
    {
        // If field filtering is needed, apply it to each event.
        return  processing_buffer->fetchWindow<MonitorEventShrdPtr>(
            [this](auto ev) -> std::optional<MonitorEventShrdPtr>
            {
                return std::make_optional(MakeMonitorEventShrdPtr(
                    ev->type, ev->message,
                    ChannelData{ev->channel_data.pv_name, filterPVField(ev->channel_data.data, this->cmd->pv_field_filter_list)}));
            });
    }
}