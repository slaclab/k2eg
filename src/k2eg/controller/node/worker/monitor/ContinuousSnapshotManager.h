#ifndef K2EG_CONTROLLER_NODE_WORKER_MONITOR_CONTINUOUSSNAPSHOTMANAGER_H_
#define K2EG_CONTROLLER_NODE_WORKER_MONITOR_CONTINUOUSSNAPSHOTMANAGER_H_

#include <k2eg/service/epics/EpicsData.h>

#include <atomic>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace k2eg::controller::node::worker::monitor {
/*
@brief ContinuousSnapshotManager is a class that manages the continuous snapshot of EPICS events.
It provides a local cache for continuous snapshots and ensures thread-safe access to the cache.
@details
The class uses a shared mutex to allow multiple threads to read from the cache simultaneously,
while ensuring exclusive access for writing. The cache is implemented as an unordered map, where the key is a string representing the snapshot name and the value is an atomic shared pointer to the MonitorEvent object.
The atomic shared pointer ensures that the MonitorEvent object can be safely accessed and modified by multiple threads without the need for additional locking mechanisms.
The class provides methods to add, remove, and retrieve snapshots from the cache.
*/
class CotninuousSnapshotManager
{
    // atomic EPICS event data shared ptr
    using MonitorEventAtomicShrdPtr = std::atomic<k2eg::service::epics_impl::MonitorEventShrdPtr>;
    
    // local cache for continuous snapshot
    mutable std::shared_mutex                                  global_cache_mutex_;
    std::unordered_map<std::string, MonitorEventAtomicShrdPtr> global_cache_;
};
} // namespace k2eg::controller::node::worker::monitor

#endif // K2EG_CONTROLLER_NODE_WORKER_MONITOR_CONTINUOUSSNAPSHOTMANAGER_H_