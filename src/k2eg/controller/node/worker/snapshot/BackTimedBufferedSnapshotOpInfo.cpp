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
    // there is nothuing to do here
    return true;
}

bool BackTimedBufferedSnapshotOpInfo::isTimeout()
{
    auto is_timeout = SnapshotOpInfo::isTimeout();
    if (is_timeout)
    {
        // swap thwe buffers so the getData can workw without
        std::unique_lock<std::shared_mutex> lock(buffer_mutex);
        std::swap(acquiring_buffer, processing_buffer);
    }
    return is_timeout;
}

void BackTimedBufferedSnapshotOpInfo::addData(MonitorEventShrdPtr event_data)
{
    std::shared_lock<std::shared_mutex> lock(buffer_mutex);
    // Add to the acquiring buffer
    acquiring_buffer->push(event_data, steady_clock::now());
}

std::vector<MonitorEventShrdPtr> BackTimedBufferedSnapshotOpInfo::getData()
{
    std::shared_lock<std::shared_mutex> lock(buffer_mutex);
    if (this->cmd->pv_field_filter_list.size() == 0)
    {
        return processing_buffer->fetchWindow();
    }
    else
    {
        // we need to filterout unneeded fileds
        return  processing_buffer->fetchWindow<MonitorEventShrdPtr>(
            [this](auto ev) -> std::optional<MonitorEventShrdPtr>
            {
                return std::make_optional(MakeMonitorEventShrdPtr(
                    ev->type, ev->message,
                    ChannelData{ev->channel_data.pv_name, filterPVField(ev->channel_data.data, this->cmd->pv_field_filter_list)}));
            });
    }
}