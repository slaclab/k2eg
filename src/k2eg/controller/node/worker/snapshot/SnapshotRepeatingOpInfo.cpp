#include <k2eg/controller/node/worker/snapshot/SnapshotRepeatingOpInfo.h>

using namespace k2eg::controller::node::worker::snapshot;
using namespace k2eg::service::epics_impl;
using namespace k2eg::controller::command::cmd;

SnapshotRepeatingOpInfo::SnapshotRepeatingOpInfo(const std::string& queue_name, ConstRepeatingSnapshotCommandShrdPtr cmd)
    : SnapshotOpInfo(queue_name, cmd), taking_data(true)
{
}

bool SnapshotRepeatingOpInfo::init(std::vector<service::epics_impl::PVShrdPtr>& sanitized_pv_name_list)
{
    // Initialize the element_data map with atomic pointers to MonitorEvent objects.
    for (const auto& sanitized_pv_name : sanitized_pv_name_list)
    {
        element_data[sanitized_pv_name->name] = std::make_shared<AtomicMonitorEventShrdPtr>();
    }
    return true;
}

bool SnapshotRepeatingOpInfo::isTimeout()
{
    // For periodic snapshots, use base class timeout logic.
    auto timeout = SnapshotOpInfo::isTimeout();

    return timeout;
}

/**
@brief This method is called when the cache is being updated for the spiecfic pv name
@details It is a virtual method that can be overridden by derived classes to perform specific actions when the cache
is updated.
*/
void SnapshotRepeatingOpInfo::addData(MonitorEventShrdPtr event_data)
{
    if (!taking_data.load(std::memory_order_acquire))
    {
        return;
    }
    auto it = element_data.find(event_data->channel_data.pv_name);
    if (it != element_data.end())
    {
        it->second->store(event_data, std::memory_order_release);
    }
}

std::vector<MonitorEventShrdPtr> SnapshotRepeatingOpInfo::getData()
{
    taking_data.store(false, std::memory_order_release);
    std::vector<MonitorEventShrdPtr> result;
    for (const auto& pair : element_data)
    {
        if (pair.second)
        {
            auto ptr = pair.second->load(std::memory_order_acquire);
            if (ptr)
                result.push_back(ptr);
        }
    }
    taking_data.store(true, std::memory_order_release);
    return result;
}