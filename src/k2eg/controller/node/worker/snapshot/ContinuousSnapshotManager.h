#ifndef K2EG_CONTROLLER_NODE_WORKER_MONITOR_CONTINUOUSSNAPSHOTMANAGER_H_
#define K2EG_CONTROLLER_NODE_WORKER_MONITOR_CONTINUOUSSNAPSHOTMANAGER_H_

#include <k2eg/common/BS_thread_pool.hpp>
#include <k2eg/common/types.h>
#include <k2eg/controller/node/worker/CommandWorker.h>
#include <k2eg/controller/node/worker/SnapshotCommandWorker.h>
#include <k2eg/service/epics/EpicsServiceManager.h>

#include <atomic>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace k2eg::controller::node::worker::snapshot {
// atomic EPICS event data shared ptr
using AtomicMonitorEventShrdPtr = std::atomic<k2eg::service::epics_impl::MonitorEventShrdPtr>;
using ShdAtomicMonitorEventShrdPtr = std::shared_ptr<AtomicMonitorEventShrdPtr>;

/*
@brief RepeatingSnapshotOpInfo is a class that holds information about a repeating snapshot operation.
@details
The class contains a command specification, after the command is epxired(ready to be fired)
all the pv data is collected using the snapshot view(this permnit to get all the lasted received data)
then it is published
*/
class RepeatingSnapshotOpInfo: public WorkerAsyncOperation
{
public:
    // keep track of the iterantion
    std::int64_t snapshot_iteration_index = 0;
    // keep track of the comamnd specification
    k2eg::controller::command::cmd::ConstSnapshotCommandShrdPtr cmd;
    // per-snapshot views hold pointers into global_cache_
    std::unordered_map<std::string, ShdAtomicMonitorEventShrdPtr> snapshot_views_;
    const std::string queue_name;
    RepeatingSnapshotOpInfo(const std::string& queue_name, k2eg::controller::command::cmd::ConstSnapshotCommandShrdPtr cmd) 
    : WorkerAsyncOperation(std::chrono::milliseconds(cmd->time_window_msec)), queue_name(queue_name), cmd(cmd) {}
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
    // define the run flag
    std::atomic<bool> run_flag = false;
    // local logger shared instances
    k2eg::service::log::ILoggerShrdPtr       logger;
    // local publisher shared instance
    k2eg::service::pubsub::IPublisherShrdPtr publisher;
    // local cache for continuous snapshot
    mutable std::shared_mutex                                     global_cache_mutex_;
    std::unordered_map<std::string, ShdAtomicMonitorEventShrdPtr> global_cache_;
    // thread pool for snapshot processing
    std::shared_ptr<BS::light_thread_pool>                thread_pool;
    // EPICS service manager
    k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager;

    // Handler's liveness token
    k2eg::common::BroadcastToken epics_handler_token;

    // Received event from EPICS IOCs that are monitored
    void epicsMonitorEvent(k2eg::service::epics_impl::EpicsServiceManagerHandlerParamterType event_received);
    // rpocess each snapshot checking if the timewindopws is epxired tahing the data from the cache
    // and publishing the data
    void processSnapshot(RepeatingSnapshotOpInfoShrdPtr snapstho_command_info);
    // Manager the reply to the client durin gthe snapshto submission
    void manageReply(const std::int8_t error_code, const std::string& error_message, k2eg::controller::command::cmd::ConstSnapshotCommandShrdPtr snapsthot_command);
    // is the callback for the publisher
    void publishEvtCB(k2eg::service::pubsub::EventType type, k2eg::service::pubsub::PublishMessage* const msg, const std::string& error_message);
public:
    ContinuousSnapshotManager(k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager);
    ~ContinuousSnapshotManager();
    void submitSnapshot(k2eg::controller::command::cmd::ConstSnapshotCommandShrdPtr snapsthot_command);
};

DEFINE_PTR_TYPES(ContinuousSnapshotManager)
} // namespace k2eg::controller::node::worker::snapshot

#endif // K2EG_CONTROLLER_NODE_WORKER_MONITOR_CONTINUOUSSNAPSHOTMANAGER_H_