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

    SnapshotRepeatingOpInfo(const std::string& queue_name, k2eg::controller::command::cmd::ConstRepeatingSnapshotCommandShrdPtr cmd);

    bool init(std::vector<service::epics_impl::PVShrdPtr>& sanitized_pv_name_list);

    bool isTimeout();

    /**
    @brief This method is called when the cache is being updated for the spiecfic pv name
    @details It is a virtual method that can be overridden by derived classes to perform specific actions when the cache
    is updated.
    */
    void addData(k2eg::service::epics_impl::MonitorEventShrdPtr event_data);
    std::vector<service::epics_impl::MonitorEventShrdPtr> getData();
};
DEFINE_PTR_TYPES(SnapshotRepeatingOpInfo)

} // namespace k2eg::controller::node::worker::snapshot

#endif // K2EG_CONTROLLER_NODE_WORKER_SNAPSHOT_SIMPLEREPEATINGSNAPSHOTOPINFO_H_