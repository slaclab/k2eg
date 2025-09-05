#ifndef K2EG_CONTROLLER_NODE_WORKER_MONITOR_CONTINUOUSSNAPSHOTMANAGER_H_
#define K2EG_CONTROLLER_NODE_WORKER_MONITOR_CONTINUOUSSNAPSHOTMANAGER_H_

#include <k2eg/service/epics/EpicsData.h>
#include <k2eg/service/metric/INodeControllerMetric.h>

#include <k2eg/common/BS_thread_pool.hpp>
#include <k2eg/common/ThrottlingManager.h>
#include <k2eg/common/types.h>

#include <k2eg/common/LockFreeBuffer.h>

#include <k2eg/service/epics/EpicsServiceManager.h>
#include <k2eg/service/metric/IMetricService.h>
#include <k2eg/service/scheduler/Scheduler.h>

#include <k2eg/controller/command/cmd/Command.h>
#include <k2eg/controller/command/cmd/SnapshotCommand.h>
#include <k2eg/controller/node/worker/CommandWorker.h>
// Intentionally avoid including SnapshotCommandWorker.h to prevent circular includes.
#include <k2eg/controller/node/worker/snapshot/SnapshotOpInfo.h>
#include <k2eg/controller/node/worker/snapshot/RepeatingSnapshotMessages.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace k2eg::controller::node::worker::snapshot {
#pragma region Types

struct RepeatingSnapshotConfiguration
{
    // thread count for snapshot processing
    size_t snapshot_processing_thread_count = 1;
};
DEFINE_PTR_TYPES(RepeatingSnapshotConfiguration)

#pragma region Defining Classes
// atomic EPICS event data shared ptr
using AtomicMonitorEventShrdPtr = std::atomic<k2eg::service::epics_impl::MonitorEventShrdPtr>;
using ShdAtomicCacheElementShrdPtr = std::shared_ptr<AtomicMonitorEventShrdPtr>;

DEFINE_UOMAP_FOR_TYPE(std::string, std::shared_ptr<SnapshotOpInfo>, RunninSnapshotMap)
using PVSnapshotMap = std::unordered_multimap<std::string, std::shared_ptr<SnapshotOpInfo>>;

struct IterationState
{
    /** @brief Number of tasks (Header/Data/Tail submissions) currently active. */
    std::atomic<int>  active_tasks{0};
    /** @brief True when Tail has been processed for this iteration. */
    std::atomic<bool> tail_processed{false};
    /** @brief Number of Data submissions still publishing for this iteration. */
    std::atomic<int>  data_pending{0};
};

class SnapshotIterationSynchronizer
{
private:
    mutable std::shared_mutex                                                     iteration_mutex_;
    std::unordered_map<std::string, uint64_t>                                     current_iteration_;
    std::unordered_map<std::string, std::atomic<bool>>                            iteration_in_progress_;
    std::unordered_map<std::string, std::unique_ptr<std::condition_variable_any>> iteration_cv_;
    std::unordered_map<std::string, std::unordered_map<uint64_t, std::shared_ptr<IterationState>>> iteration_states_;

    // Private helper to release the lock and notify the next waiting iteration.
    void releaseLock(const std::string& snapshot_name, uint64_t iteration_id_to_clear);

public:
    // Default constructor and destructor.
    SnapshotIterationSynchronizer() = default;

    // Deleted copy constructor and assignment operator to prevent copying.
    ~SnapshotIterationSynchronizer() = default;

    // Reserve an iteration number and wait if the previous iteration is still in progress.
    int64_t acquireIteration(const std::string& snapshot_name);

    // A task calls this to announce it has started.
    void startTask(const std::string& snapshot_name, uint64_t iteration_id);

    // A task calls this to announce it has finished.
    void finishTask(const std::string& snapshot_name, uint64_t iteration_id);

    // The Tail task calls this to mark the logical end of the iteration.
    void markTailProcessed(const std::string& snapshot_name, uint64_t iteration_id);

    // Data submissions draining handled within SnapshotOpInfo

    // Clean up when a snapshot is removed entirely.
    void removeSnapshot(const std::string& snapshot_name);
};

// RAII helper to ensure startTask/finishTask are always paired for exception safety.
class TaskGuard
{
public:
    TaskGuard(SnapshotIterationSynchronizer& sync, const std::string& name, uint64_t id)
        : sync_(sync), name_(name), id_(id)
    {
        sync_.startTask(name_, id_);
    }

    ~TaskGuard()
    {
        sync_.finishTask(name_, id_);
    }

private:
    SnapshotIterationSynchronizer& sync_;
    const std::string&             name_;
    uint64_t                       id_;
};

// class used to submit data to the publisher
class SnapshotSubmissionTask
{
    bool                                     last_submission = false; // flag to indicate if the task should close
    std::shared_ptr<SnapshotOpInfo>          snapshot_command_info; // shared pointer to the snapshot operation info
    SnapshotSubmissionShrdPtr                submission_shrd_ptr;
    k2eg::service::pubsub::IPublisherShrdPtr publisher;
    k2eg::service::log::ILoggerShrdPtr       logger;
    SnapshotIterationSynchronizer&           iteration_sync_;

public:
    SnapshotSubmissionTask(std::shared_ptr<SnapshotOpInfo> snapshot_command_info, SnapshotSubmissionShrdPtr submission_shrd_ptr, k2eg::service::pubsub::IPublisherShrdPtr publisher, k2eg::service::log::ILoggerShrdPtr logger, SnapshotIterationSynchronizer& iteration_sync, bool last_submission = false);

    void operator()();
};

/*
@brief ContinuousSnapshotManager is a class that manages the continuous snapshot of EPICS events.
It provides a local cache for continuous snapshots and ensures thread-safe access to the cache.
@details
The class uses a shared mutex to allow multiple threads to read from the cache simultaneously,
while ensuring exclusive access for writing. The cache is implemented as an unordered map, where the key is a string
representing the snapshot name and the value is an atomic shared pointer to the MonitorEvent object. The atomic shared
pointer ensures that the MonitorEvent object can be safely accessed and modified by multiple threads without the need
for additional locking mechanisms. The class provides methods to add, remove, and retrieve snapshots from the cache.
*/
class ContinuousSnapshotManager
{
    // Reference to configuration for repeating snapshots (thread count, etc.)
    const RepeatingSnapshotConfiguration& repeating_snapshot_configuration;

    // Flag indicating if the manager is running
    std::atomic<bool> run_flag = false;

    // Shared pointer to the logger instance
    k2eg::service::log::ILoggerShrdPtr logger;

    // Shared pointer to the publisher instance for publishing snapshot data
    k2eg::service::pubsub::IPublisherShrdPtr publisher;

    // Mutex for synchronizing access to running snapshots
    mutable std::shared_mutex snapshot_running_mutex_;

    // Map of currently running snapshots (snapshot name -> SnapshotOpInfo)
    RunninSnapshotMap snapshot_running_;

    // Multimap of PV names to their associated snapshot operations
    // multiple snapshots can be associated with the same PV
    PVSnapshotMap pv_snapshot_map_;

    // Thread pool for processing snapshot operations
    std::shared_ptr<BS::light_thread_pool> thread_pool;

    // Shared pointer to the EPICS service manager
    k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager;

    // Token for managing the liveness of EPICS event handlers
    k2eg::common::BroadcastToken epics_handler_token;

    // Reference to node controller metrics for statistics collection
    k2eg::service::metric::INodeControllerMetric& metrics;

    std::thread                                    expiration_thread;
    std::atomic<bool>                              expiration_thread_running{false};
    std::unique_ptr<SnapshotIterationSynchronizer> iteration_sync_;

    // Tracks the currently active iteration id per snapshot name during scheduling.
    std::unordered_map<std::string, int64_t> active_iteration_id_;

    /**
     * @brief Callback for receiving EPICS monitor events.
     * @param event_received The event data received from EPICS IOCs.
     */
    void epicsMonitorEvent(k2eg::service::epics_impl::EpicsServiceManagerHandlerParamterType event_received);

    /**
     * @brief Process a snapshot operation, checking time windows and publishing data.
     * @param snapshot_command_info Shared pointer to the snapshot operation info.
     */
    void processSnapshot(SnapshotOpInfoShrdPtr snapshot_command_info);

    /**
     * @brief Manage the reply to the client during snapshot submission.
     * @param error_code Error code to report.
     * @param error_message Error message string.
     * @param command The command associated with the reply.
     * @param publishing_topic Optional topic for publishing the reply.
     */
    void manageReply(const std::int8_t error_code, const std::string& error_message, k2eg::controller::command::cmd::ConstCommandShrdPtr command, const std::string& publishing_topic = "");

    /**
     * @brief Callback for publisher events.
     * @param type Event type.
     * @param msg Pointer to the published message.
     * @param error_message Error message string, if any.
     */
    void publishEvtCB(k2eg::service::pubsub::EventType type, k2eg::service::pubsub::PublishMessage* const msg, const std::string& error_message);

    /**
     * @brief Start a new repeating snapshot operation.
     * @param command Shared pointer to the repeating snapshot command.
     */
    void startSnapshot(command::cmd::ConstRepeatingSnapshotCommandShrdPtr command);

    /**
     * @brief Trigger an existing repeating snapshot operation.
     * @param command Shared pointer to the trigger command.
     */
    void triggerSnapshot(command::cmd::ConstRepeatingSnapshotTriggerCommandShrdPtr command);

    /**
     * @brief Stop a running repeating snapshot operation.
     * @param command Shared pointer to the stop command.
     */
    void stopSnapshot(command::cmd::ConstRepeatingSnapshotStopCommandShrdPtr command);

    /**
     * @brief Handle statistics collection for the snapshot engine.
     * @param task_properties Properties of the scheduled statistics task.
     */
    void handleStatistic(k2eg::service::scheduler::TaskProperties& task_properties);
    void expirationCheckerLoop();

public:
    /**
     * @brief Construct a ContinuousSnapshotManager.
     * @param repeating_snapshot_configuration Reference to the repeating snapshot configuration.
     * @param epics_service_manager Shared pointer to the EPICS service manager.
     */
    ContinuousSnapshotManager(const RepeatingSnapshotConfiguration& repeating_snapshot_configuration, k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager);

    /**
     * @brief Destructor for ContinuousSnapshotManager.
     */
    ~ContinuousSnapshotManager();

    /**
     * @brief Submit a new snapshot command for processing.
     * @param snapsthot_command Shared pointer to the snapshot command.
     */
    void submitSnapshot(k2eg::controller::command::cmd::ConstCommandShrdPtr snapsthot_command);

    /**
     * @brief Get the number of currently running snapshots.
     * @return The count of running snapshots.
     */
    std::size_t getRunningSnapshotCount() const;
};

DEFINE_PTR_TYPES(ContinuousSnapshotManager)
} // namespace k2eg::controller::node::worker::snapshot

#endif // K2EG_CONTROLLER_NODE_WORKER_MONITOR_CONTINUOUSSNAPSHOTMANAGER_H_
