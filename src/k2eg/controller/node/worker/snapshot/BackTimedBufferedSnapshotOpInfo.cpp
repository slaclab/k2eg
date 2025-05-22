#include <k2eg/controller/node/worker/snapshot/BackTimedBufferedSnapshotOpInfo.h>

using namespace k2eg::controller::node::worker::snapshot;
using namespace k2eg::controller::command::cmd;
using namespace k2eg::service::epics_impl;
using namespace std::chrono;

BackTimedBufferedSnapshotOpInfo::BackTimedBufferedSnapshotOpInfo(const std::string& queue_name, ConstRepeatingSnapshotCommandShrdPtr cmd)
    : SnapshotOpInfo(queue_name, cmd), buffer(cmd->time_window_msec),taking_data(true)
{
}

bool BackTimedBufferedSnapshotOpInfo::init(std::vector<PVShrdPtr>& sanitized_pv_name_list){
    // there is nothuing to do here
    return true;
}

void BackTimedBufferedSnapshotOpInfo::addData(MonitorEventShrdPtr event_data)
{
    if (!taking_data.load(std::memory_order_acquire))
    {
        return;
    }
    // add to the buffer
    buffer.push(event_data, steady_clock::now());
}

std::vector<MonitorEventShrdPtr> BackTimedBufferedSnapshotOpInfo::getData()
{
    taking_data.store(false, std::memory_order_release);
    auto result = buffer.fetchWindow();
    taking_data.store(true, std::memory_order_release);
    return result;
}
