#include "k2eg/service/epics/EpicsData.h"
#include <k2eg/controller/node/worker/snapshot/BackTimedBufferedSnapshotOpInfo.h>

using namespace k2eg::controller::node::worker::snapshot;
using namespace k2eg::controller::command::cmd;
using namespace k2eg::service::epics_impl;
using namespace std::chrono;

BackTimedBufferedSnapshotOpInfo::BackTimedBufferedSnapshotOpInfo(const std::string& queue_name, ConstRepeatingSnapshotCommandShrdPtr cmd)
    : SnapshotOpInfo(queue_name, cmd), taking_data(true), buffer(cmd->time_window_msec)
{
}

bool BackTimedBufferedSnapshotOpInfo::init(std::vector<PVShrdPtr>& sanitized_pv_name_list)
{
    // there is nothuing to do here
    return true;
}

void BackTimedBufferedSnapshotOpInfo::addData(MonitorEventShrdPtr event_data)
{
    if (!taking_data.load(std::memory_order_acquire))
    {
        return;
    }
    std::lock_guard<std::mutex> lock(buffer_mutex);
    // add to the buffer
    buffer.push(event_data, steady_clock::now());
}

std::vector<MonitorEventShrdPtr> BackTimedBufferedSnapshotOpInfo::getData()
{
    std::vector<MonitorEventShrdPtr> result;
    taking_data.store(false, std::memory_order_release);
    std::lock_guard<std::mutex> lock(buffer_mutex);
    
    if (this->cmd->pv_field_filter_list.size() == 0)
    {
        result = buffer.fetchWindow();
    }
    else
    {
        // we need to filterout unneeded fileds
        result = buffer.fetchWindow<MonitorEventShrdPtr>(
            [this](auto ev) -> std::optional<MonitorEventShrdPtr>
            {
                return std::make_optional(MakeMonitorEventShrdPtr(
                    ev->type, ev->message,
                    ChannelData{ev->channel_data.pv_name, filterPVField(ev->channel_data.data, this->cmd->pv_field_filter_list)}));
            });
    }
    taking_data.store(true, std::memory_order_release);
    return result;
}