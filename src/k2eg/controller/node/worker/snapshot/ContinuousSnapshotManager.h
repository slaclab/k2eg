#ifndef K2EG_CONTROLLER_NODE_WORKER_MONITOR_CONTINUOUSSNAPSHOTMANAGER_H_
#define K2EG_CONTROLLER_NODE_WORKER_MONITOR_CONTINUOUSSNAPSHOTMANAGER_H_

#include <k2eg/common/BS_thread_pool.hpp>
#include <k2eg/common/LockFreeBuffer.h>
#include <k2eg/common/ThrottlingManager.h>
#include <k2eg/common/types.h>

#include <k2eg/service/configuration/INodeConfiguration.h>
#include <k2eg/service/epics/EpicsData.h>
#include <k2eg/service/epics/EpicsServiceManager.h>
#include <k2eg/service/metric/IMetricService.h>
#include <k2eg/service/metric/INodeControllerMetric.h>
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
    std::atomic<int> active_tasks{0};
    /** @brief True when Tail has been processed for this iteration. */
    std::atomic<bool> tail_processed{false};
    /** @brief Number of Data submissions still publishing for this iteration. */
    std::atomic<int> data_pending{0};
};

/**
 * @brief Per-snapshot iteration synchronizer.
 * @details Ensures sequential processing of iterations per snapshot name while
 *          allowing concurrent work across different snapshots. Tracks active
 *          tasks within an iteration and notifies when Tail completes so the
 *          next iteration can proceed. Thread-safe.
 */
class SnapshotIterationSynchronizer
{
private:
    mutable std::shared_mutex                                                                      iteration_mutex_; ///< Mutex for synchronizing access to iteration state.
    std::unordered_map<std::string, uint64_t>                                                      current_iteration_;
    std::unordered_map<std::string, std::atomic<bool>>                                             iteration_in_progress_;
    std::unordered_map<std::string, std::unique_ptr<std::condition_variable_any>>                  iteration_cv_;
    std::unordered_map<std::string, std::unordered_map<uint64_t, std::shared_ptr<IterationState>>> iteration_states_;

    // Private helper to release the lock and notify the next waiting iteration.
    void releaseLock(const std::string& snapshot_name, uint64_t iteration_id_to_clear);

public:
    /** @brief Default constructor. */
    SnapshotIterationSynchronizer() = default;
    /** @brief Default destructor. */
    ~SnapshotIterationSynchronizer() = default;

    /**
     * @brief Reserve a new iteration id for a snapshot.
     * @details Blocks if the previous iteration is still in progress.
     * @param snapshot_name Snapshot name.
     * @return Acquired iteration id.
     */
    int64_t acquireIteration(const std::string& snapshot_name);

    /**
     * @brief Announce task start for an iteration.
     * @param snapshot_name Snapshot name.
     * @param iteration_id Iteration id.
     */
    void startTask(const std::string& snapshot_name, uint64_t iteration_id);

    /**
     * @brief Announce task completion for an iteration.
     * @param snapshot_name Snapshot name.
     * @param iteration_id Iteration id.
     */
    void finishTask(const std::string& snapshot_name, uint64_t iteration_id);

    /**
     * @brief Mark Tail processed for an iteration.
     * @param snapshot_name Snapshot name.
     * @param iteration_id Iteration id.
     */
    void markTailProcessed(const std::string& snapshot_name, uint64_t iteration_id);

    // Data submissions draining is handled within SnapshotOpInfo

    /**
     * @brief Remove all state for a snapshot.
     * @param snapshot_name Snapshot name.
     */
    void removeSnapshot(const std::string& snapshot_name);

    /**
     * @brief Notify all condition variables to wake any waiting threads. Used during shutdown.
     */
    void notifyAll();
};

// RAII helper to ensure startTask/finishTask are always paired for exception safety.
/**
 * @brief RAII helper that pairs startTask/finishTask calls.
 * @details Guarantees proper decrement in the presence of exceptions.
 */
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

/**
 * @brief Publishes a single SnapshotSubmission to the output topic.
 * @details Handles header/data/tail ordering constraints and metrics logging.
 */
class SnapshotSubmissionTask
{
    bool                                     last_submission = false; // flag to indicate if the task should close
    std::shared_ptr<SnapshotOpInfo>          snapshot_command_info;   // shared pointer to the snapshot operation info
    SnapshotSubmissionShrdPtr                submission_shrd_ptr;
    k2eg::service::pubsub::IPublisherShrdPtr publisher;
    k2eg::service::log::ILoggerShrdPtr       logger;
    SnapshotIterationSynchronizer&           iteration_sync_;

public:
    SnapshotSubmissionTask(std::shared_ptr<SnapshotOpInfo> snapshot_command_info, SnapshotSubmissionShrdPtr submission_shrd_ptr, k2eg::service::pubsub::IPublisherShrdPtr publisher, k2eg::service::log::ILoggerShrdPtr logger, SnapshotIterationSynchronizer& iteration_sync, bool last_submission = false);

    /**
     * @brief Execute the publishing task.
     */
    void operator()();
};

/**
 * @brief Orchestrates repeating/triggered snapshot operations.
 * @details Manages the lifecycle of snapshot operations, wiring EPICS monitor
 *          events to SnapshotOpInfo implementations, scheduling windows, and
 *          publishing header/data/tail messages in-order per iteration.
 *          Thread-safety: uses a shared mutex to allow concurrent reads of the
 *          running-snapshots map and exclusive writes on updates. Iteration
 *          ordering is enforced via SnapshotIterationSynchronizer.
 */
class ContinuousSnapshotManager
{
    // Reference to configuration for repeating snapshots (thread count, etc.)
    const RepeatingSnapshotConfiguration& repeating_snapshot_configuration;

    // Flag indicating if the manager is running
    std::atomic<bool> run_flag = false;

    // Shared pointer to the logger instance
    k2eg::service::log::ILoggerShrdPtr logger;

    // Node configuration service
    service::configuration::INodeConfigurationShrdPtr node_configuration;

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
    std::shared_ptr<BS::light_thread_pool> thread_pool_submitting;

    // Thread pool for DAQ processing (if needed)
    std::shared_ptr<BS::light_thread_pool> thread_pool_daq_processing;

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

    /**
     * @brief Loop to check for expired snapshots and clean them up.
     */
    void expirationCheckerLoop();

    /**
     * @brief Try to configure the start of a snapshot.
     * @param snapshot_ops_info Shared pointer to the snapshot operation info.
     * @return True if the configuration was successful and snapshot can start, false otherwise.
     */
    bool tryToConfigureSnapshotStart(SnapshotOpInfo& snapshot_ops_info);

    /**
     * @brief Set the snapshot as running in the configuration.
     * @param snapshot_ops_info Shared pointer to the snapshot operation info.
     * @return True if the snapshot was successfully set as running, false otherwise.
     */
    bool releaseSnapshotForStop(SnapshotOpInfo& snapshot_ops_info);

    /**
     * @brief Handle the periodic task for snapshot management.
     * @param task_properties Properties of the scheduled periodic task.
     */
    void handlePeriodicTask(k2eg::service::scheduler::TaskProperties& task_properties);

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
