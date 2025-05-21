#ifndef K2EG_CONTROLLER_NODE_WORKER_SNAPSHOT_SIMPLEREPEATINGSNAPSHOTOPINFO_H_
#define K2EG_CONTROLLER_NODE_WORKER_SNAPSHOT_SIMPLEREPEATINGSNAPSHOTOPINFO_H_

#include <atomic>
#include <k2eg/common/types.h>
#include <k2eg/controller/node/worker/snapshot/SnapshotOpInfo.h>

namespace k2eg::controller::node::worker::snapshot {
using AtomicMonitorEventShrdPtr = std::atomic<k2eg::service::epics_impl::MonitorEventShrdPtr>;
using ShdAtomicCacheElementShrdPtr = std::shared_ptr<AtomicMonitorEventShrdPtr>;

/*
@brief RepeatingSnapshotOpInfo is a class that holds information about a repeating snapshot operation.
@details
The class contains a command specification, after the command is epxired(ready to be fired)
all the pv data is collected using the snapshot view(this permnit to get all the lasted received data)
then it is published
*/
class SnapshotRepeatingOpInfo : public SnapshotOpInfo
{
public:
    // per-snapshot views hold pointers into global_cache_
    std::unordered_map<std::string, ShdAtomicCacheElementShrdPtr> element_data;

    // define when the snapshot is acquiring data
    std::atomic<bool> taking_data;

    SnapshotRepeatingOpInfo(const std::string& queue_name, k2eg::controller::command::cmd::ConstRepeatingSnapshotCommandShrdPtr cmd)
        : SnapshotOpInfo(queue_name, cmd)
        ,taking_data(true)
    {
    }

    bool init(std::vector<service::epics_impl::PVShrdPtr>& sanitized_pv_name_list)
    {
        // Initialize the element_data map with atomic pointers to MonitorEvent objects.
        for (const auto& sanitized_pv_name : sanitized_pv_name_list)
        {
            element_data[sanitized_pv_name->name] = std::make_shared<AtomicMonitorEventShrdPtr>();
        }
        return true;
    }

    bool isTimeout()
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
    virtual void addData(k2eg::service::epics_impl::MonitorEventShrdPtr event_data)
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

    virtual std::vector<service::epics_impl::MonitorEventShrdPtr> getData()
    {
        taking_data.store(false, std::memory_order_release);
        std::vector<service::epics_impl::MonitorEventShrdPtr> result(element_data.size());
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
};
DEFINE_PTR_TYPES(SnapshotRepeatingOpInfo)

} // namespace k2eg::controller::node::worker::snapshot

#endif // K2EG_CONTROLLER_NODE_WORKER_SNAPSHOT_SIMPLEREPEATINGSNAPSHOTOPINFO_H_