#ifndef K2EG_CONTROLLER_NODE_WORKER_MONITOR_CONTINUOUSSNAPSHOTMANAGER_H_
#define K2EG_CONTROLLER_NODE_WORKER_MONITOR_CONTINUOUSSNAPSHOTMANAGER_H_

#include "k2eg/common/types.h"
#include <k2eg/common/BS_thread_pool.hpp>
#include <k2eg/controller/node/worker/CommandWorker.h>
#include <k2eg/controller/node/worker/SnapshotCommandWorker.h>
#include <k2eg/service/epics/EpicsServiceManager.h>

#include <atomic>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace k2eg::controller::node::worker::snapshot {

/*
@brief RepeatingSnapshotOpInfo is a class that holds information about a repeating snapshot operation.
@details
The class contains a command specification, after the command is epxired(ready to be fired)
all the pv data is collected using the snapshot view(this permnit to get all the lasted received data)
then it is published
*/
class RepeatingSnapshotOpInfo
{
public:
    // keep track of the iterantion
    std::int64_t snapshot_iteration_index = 0;
    // keep track of the comamnd specification
    k2eg::controller::command::cmd::ConstSnapshotCommandShrdPtr cmd;
    // per-snapshot views hold pointers into global_cache_
    std::unordered_map<std::string, std::vector<k2eg::service::epics_impl::MonitorEventShrdPtr>> snapshot_views_;

    RepeatingSnapshotOpInfo(k2eg::controller::command::cmd::ConstSnapshotCommandShrdPtr cmd) : cmd(cmd) {}
};
DEFINE_PTR_TYPES(RepeatingSnapshotOpInfo)

/*
@brief ContinuousSnapshotManager is a class that manages the continuous snapshot of EPICS events.
It provides a local cache for continuous snapshots and ensures thread-safe access to the cache.
@details
The class uses a shared mutex to allow multiple threads to read from the cache simultaneously,
while ensuring exclusive access for writing. The cache is implemented as an unordered map, where the key is a string
representing the snapshot name and the value is an atomic shared pointer to the MonitorEvent object. The atomic shared
pointer ensures that the MonitorEvent object can be safely accessed and modified by multiple threads without the need
for additional locking mechanisms. The class provides methods to add, remove, and retrieve snapshots from the cache.

Data is published se sequentially on publisher identifyed by name and iteration number
*/
class ContinuousSnapshotManager
{
    // atomic EPICS event data shared ptr
    using MonitorEventAtomicShrdPtr = std::atomic<k2eg::service::epics_impl::MonitorEventShrdPtr>;

    // local cache for continuous snapshot
    mutable std::shared_mutex                                  global_cache_mutex_;
    std::unordered_map<std::string, MonitorEventAtomicShrdPtr> global_cache_;

    std::shared_ptr<BS::light_thread_pool>                thread_pool;
    k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager;
    void epicsMonitorEvent(k2eg::service::epics_impl::EpicsServiceManagerHandlerParamterType event_received);
    void manageSnapshot(k2eg::controller::command::cmd::ConstCommandShrdPtr command);

public:
    ContinuousSnapshotManager(k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager);
    ~ContinuousSnapshotManager() = default;
    virtual void processCommand(k2eg::controller::command::cmd::ConstSnapshotCommandShrdPtr command);
};

DEFINE_PTR_TYPES(ContinuousSnapshotManager)
} // namespace k2eg::controller::node::worker::snapshot

#endif // K2EG_CONTROLLER_NODE_WORKER_MONITOR_CONTINUOUSSNAPSHOTMANAGER_H_