#include "k2eg/controller/node/worker/snapshot/SnapshotOpInfo.h"
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
    // Initialize the element_data map with atomic pointers to MonitorEvent objects for each PV.
    for (const auto& sanitized_pv_name : sanitized_pv_name_list)
    {
        element_data[sanitized_pv_name->name] = std::make_shared<AtomicMonitorEventShrdPtr>();
    }
    return true;
}

bool SnapshotRepeatingOpInfo::isTimeout(const std::chrono::steady_clock::time_point& now)
{
    // Use base class timeout logic for periodic snapshots.
    auto timeout = SnapshotOpInfo::isTimeout(now);
    return timeout;
}

void SnapshotRepeatingOpInfo::addData(MonitorEventShrdPtr event_data)
{
    // Only store data if currently taking data for this snapshot.
    if (!taking_data.load(std::memory_order_acquire))
    {
        return;
    }
    // Store the latest event for the corresponding PV.
    auto it = element_data.find(event_data->channel_data.pv_name);
    if (it != element_data.end())
    {
        it->second->store(event_data, std::memory_order_release);
    }
}

SnapshotSubmissionShrdPtr SnapshotRepeatingOpInfo::getData()
{
    // Temporarily stop taking data while collecting the snapshot.
    taking_data.store(false, std::memory_order_release);
    std::vector<MonitorEventShrdPtr> result;
    auto                             submission_ts = std::chrono::steady_clock::now();
    for (const auto& pair : element_data)
    {
        if (pair.second)
        {
            // Collect the latest event for each PV if available.
            auto ptr = pair.second->load(std::memory_order_acquire);
            if (ptr)
                result.push_back(ptr);
        }
    }
    // Resume taking data after snapshot collection.
    taking_data.store(true, std::memory_order_release);
    return MakeSnapshotSubmissionShrdPtr(
        submission_ts,
        submission_ts,
        std::move(result),
        (SnapshotSubmissionType::Header | SnapshotSubmissionType::Data | SnapshotSubmissionType::Tail),
        0 // scheduler assigns iteration id
    );
}
