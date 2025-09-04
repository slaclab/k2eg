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
#include <k2eg/controller/node/worker/SnapshotCommandWorker.h>
#include <k2eg/controller/node/worker/snapshot/SnapshotOpInfo.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace k2eg::controller::node::worker::snapshot {
#pragma region Types

/**
 * @struct RepeatingSnaptshotConfiguration
 * @brief Configuration for repeating snapshot processing.
 * @details Controls internal resources used to process snapshot submissions.
 */
struct RepeatingSnaptshotConfiguration
{
    size_t snapshot_processing_thread_count = 1; /**< Number of threads used to process snapshot submissions. */

    /**
     * @brief Serialize configuration to a compact string for logging.
     * @return Human-readable key=value summary.
     */
    const std::string toString() const
    {
        return std::format("RepeatingSnaptshotConfiguration(snapshot_processing_thread_count={})", snapshot_processing_thread_count);
    }
};
DEFINE_PTR_TYPES(RepeatingSnaptshotConfiguration)

/**
 * @brief Repeating snapshot message header.
 * @details Carries metadata for a snapshot iteration. All messages that belong
 *          to the same iteration are published sequentially to the same topic/partition.
 */
struct RepeatingSnaptshotHeader
{
    const std::int8_t  message_type = 0; /**< Discriminator for header messages. */
    const std::string  snapshot_name;    /**< Snapshot name associated with this iteration. */
    const std::int64_t timestamp;        /**< Snapshot creation timestamp (epoch millis). */
    const std::int64_t iteration_index;  /**< Monotonic iteration index assigned by the manager. */
};
DEFINE_PTR_TYPES(RepeatingSnaptshotHeader)

#pragma region Serialization

/**
 * @brief Repeating snapshot PV data message.
 * @details Conveys the values for a specific PV belonging to an iteration.
 */
struct RepeatingSnaptshotData
{
    const std::int8_t                                  message_type = 1; /**< Discriminator for data messages. */
    const std::int64_t                                 timestamp;        /**< Event timestamp for this PV sample (epoch millis). */
    const std::int64_t                                 header_timestamp; /**< Timestamp of the corresponding iteration header (epoch millis). */
    const std::int64_t                                 iteration_index;  /**< Iteration index matching the header. */
    k2eg::service::epics_impl::ConstChannelDataShrdPtr pv_data;          /**< Serialized PV payload. */
};
DEFINE_PTR_TYPES(RepeatingSnaptshotData)

/**
 * @brief Repeating snapshot completion message.
 * @details Marks the end of an iteration submission with optional error info.
 */
struct RepeatingSnaptshotCompletion
{
    const std::int8_t  message_type = 2; /**< Discriminator for completion messages. */
    const std::int32_t error;            /**< Error code (0 for success). */
    const std::string  error_message;    /**< Optional error description; empty on success. */
    const std::string  snapshot_name;    /**< Snapshot name associated with this iteration. */
    const std::int64_t timestamp;        /**< Completion timestamp (epoch millis). */
    const std::int64_t header_timestamp; /**< Timestamp of the corresponding iteration header (epoch millis). */
    const std::int64_t iteration_index;  /**< Iteration index matching the header. */
};
DEFINE_PTR_TYPES(RepeatingSnaptshotCompletion)

/**
 * @brief Serialize a snapshot header to JSON.
 * @param event_header Header to serialize.
 * @param json_message Output buffer.
 */
inline void serializeJson(const RepeatingSnaptshotHeader& event_header, common::JsonMessage& json_message)
{
    json_message.getJsonObject()["message_type"] = event_header.message_type;
    json_message.getJsonObject()["iter_index"] = event_header.iteration_index;
    json_message.getJsonObject()["snapshot_name"] = event_header.snapshot_name;
    json_message.getJsonObject()["timestamp"] = event_header.timestamp;
}

/**
 * @brief Serialize PV data to JSON.
 * @param event_data Data to serialize.
 * @param json_message Output buffer.
 */
inline void serializeJson(const RepeatingSnaptshotData& event_data, common::JsonMessage& json_message)
{
    json_message.getJsonObject()["message_type"] = event_data.message_type;
    json_message.getJsonObject()["iter_index"] = event_data.iteration_index;
    json_message.getJsonObject()["timestamp"] = event_data.timestamp;
    json_message.getJsonObject()["header_timestamp"] = event_data.header_timestamp;
    service::epics_impl::epics_serializer_factory.resolve(common::SerializationType::JSON)->serialize(*event_data.pv_data, json_message);
}

/**
 * @brief Serialize a completion message to JSON.
 * @param event_completion Completion to serialize.
 * @param json_message Output buffer.
 */
inline void serializeJson(const RepeatingSnaptshotCompletion& event_completion, common::JsonMessage& json_message)
{
    json_message.getJsonObject()["message_type"] = event_completion.message_type;
    json_message.getJsonObject()["error"] = event_completion.error;
    json_message.getJsonObject()["error_message"] = event_completion.error_message;
    json_message.getJsonObject()["iter_index"] = event_completion.iteration_index;
    json_message.getJsonObject()["timestamp"] = event_completion.timestamp;
    json_message.getJsonObject()["header_timestamp"] = event_completion.header_timestamp;
    json_message.getJsonObject()["snapshot_name"] = event_completion.snapshot_name;
}

/**
 * @brief Serialize a snapshot header to MessagePack.
 * @param header_event Header to serialize.
 * @param msgpack_message Output buffer.
 * @param map_size Unused; preserved for signature compatibility.
 */
inline void serializeMsgpack(const RepeatingSnaptshotHeader& header_event, common::MsgpackMessage& msgpack_message, std::uint8_t map_size = 0)
{
    msgpack::packer<msgpack::sbuffer> packer(msgpack_message.getBuffer());
    packer.pack_map(4);
    packer.pack("message_type");
    packer.pack(header_event.message_type);
    packer.pack("snapshot_name");
    packer.pack(header_event.snapshot_name);
    packer.pack("timestamp");
    packer.pack(header_event.timestamp);
    packer.pack("iter_index");
    packer.pack(header_event.iteration_index);
}

/**
 * @brief Serialize PV data to MessagePack.
 * @param data_event Data to serialize.
 * @param msgpack_message Output buffer.
 * @param map_size Unused; preserved for signature compatibility.
 */
inline void serializeMsgpack(const RepeatingSnaptshotData& data_event, common::MsgpackMessage& msgpack_message, std::uint8_t map_size = 0)
{
    msgpack::packer<msgpack::sbuffer> packer(msgpack_message.getBuffer());
    packer.pack_map(5);
    packer.pack("message_type");
    packer.pack(data_event.message_type);
    packer.pack("timestamp");
    packer.pack(data_event.timestamp);
    packer.pack("header_timestamp");
    packer.pack(data_event.header_timestamp);
    packer.pack("iter_index");
    packer.pack(data_event.iteration_index);
    service::epics_impl::epics_serializer_factory.resolve(common::SerializationType::Msgpack)->serialize(*data_event.pv_data, msgpack_message);
}

/**
 * @brief Serialize a completion message to MessagePack.
 * @param header_completion Completion to serialize.
 * @param msgpack_message Output buffer.
 * @param map_size Unused; preserved for signature compatibility.
 */
inline void serializeMsgpack(const RepeatingSnaptshotCompletion& header_completion, common::MsgpackMessage& msgpack_message, std::uint8_t map_size = 0)
{
    msgpack::packer<msgpack::sbuffer> packer(msgpack_message.getBuffer());
    packer.pack_map(header_completion.error_message.empty() ? 6 : 7);
    packer.pack("message_type");
    packer.pack(header_completion.message_type);
    packer.pack("error");
    packer.pack(header_completion.error);
    if (header_completion.error_message.empty() == false)
    {
        packer.pack("error_message");
        packer.pack(header_completion.error_message);
    }
    packer.pack("iter_index");
    packer.pack(header_completion.iteration_index);
    packer.pack("timestamp");
    packer.pack(header_completion.timestamp);
    packer.pack("header_timestamp");
    packer.pack(header_completion.header_timestamp);
    packer.pack("snapshot_name");
    packer.pack(header_completion.snapshot_name);
}

/**
 * @brief Serialize a header using the requested format.
 * @param header Header to serialize.
 * @param type Serialization format.
 * @return Serialized message buffer, or null on unsupported type.
 */
inline k2eg::common::SerializedMessageShrdPtr serialize(const RepeatingSnaptshotHeader& header, k2eg::common::SerializationType type)
{
    switch (type)
    {
    case k2eg::common::SerializationType::JSON:
        {
            auto json_message = std::make_shared<k2eg::common::JsonMessage>();
            serializeJson(header, *json_message);
            return json_message;
        }
    case k2eg::common::SerializationType::Msgpack:
        {
            auto msgpack_message = std::make_shared<k2eg::common::MsgpackMessage>();
            serializeMsgpack(header, *msgpack_message);
            return msgpack_message;
        }
    default: return nullptr;
    }
}

/**
 * @brief Serialize PV data using the requested format.
 * @param data Data to serialize.
 * @param type Serialization format.
 * @return Serialized message buffer, or null on unsupported type.
 */
inline k2eg::common::SerializedMessageShrdPtr serialize(const RepeatingSnaptshotData& data, k2eg::common::SerializationType type)
{
    switch (type)
    {
    case k2eg::common::SerializationType::JSON:
        {
            auto json_message = std::make_shared<k2eg::common::JsonMessage>();
            serializeJson(data, *json_message);
            return json_message;
        }
    case k2eg::common::SerializationType::Msgpack:
        {
            auto msgpack_message = std::make_shared<k2eg::common::MsgpackMessage>();
            serializeMsgpack(data, *msgpack_message);
            return msgpack_message;
        }
    default: return nullptr;
    }
}

/**
 * @brief Serialize a completion message using the requested format.
 * @param data Completion to serialize.
 * @param type Serialization format.
 * @return Serialized message buffer, or null on unsupported type.
 */
inline k2eg::common::SerializedMessageShrdPtr serialize(const RepeatingSnaptshotCompletion& data, k2eg::common::SerializationType type)
{
    switch (type)
    {
    case k2eg::common::SerializationType::JSON:
        {
            auto json_message = std::make_shared<k2eg::common::JsonMessage>();
            serializeJson(data, *json_message);
            return json_message;
        }
    case k2eg::common::SerializationType::Msgpack:
        {
            auto msgpack_message = std::make_shared<k2eg::common::MsgpackMessage>();
            serializeMsgpack(data, *msgpack_message);
            return msgpack_message;
        }
    default: return nullptr;
    }
}

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
    bool                                     last_submission = false; /**< True if this is the last submission for the iteration. */
    std::shared_ptr<SnapshotOpInfo>          snapshot_command_info;   /**< Owning snapshot operation context. */
    SnapshotSubmissionShrdPtr                submission_shrd_ptr;     /**< Submission to publish. */
    k2eg::service::pubsub::IPublisherShrdPtr publisher;               /**< Destination publisher. */
    k2eg::service::log::ILoggerShrdPtr       logger;                  /**< Logger instance. */
    SnapshotIterationSynchronizer&           iteration_sync_;         /**< Per-snapshot iteration synchronizer. */

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
    const RepeatingSnaptshotConfiguration& repeating_snapshot_configuration; ///< Repeating snapshot configuration (resources).

    std::atomic<bool>                                     run_flag = false;                 ///< True while the manager is running.
    k2eg::service::log::ILoggerShrdPtr                    logger;                           ///< Logger instance.
    service::configuration::INodeConfigurationShrdPtr     node_configuration;               ///< Node configuration (topics, limits, etc.).
    k2eg::service::pubsub::IPublisherShrdPtr              publisher;                        ///< Publisher for outgoing snapshot messages.
    mutable std::shared_mutex                             snapshot_runinnig_mutex_;         ///< Protects access to running snapshot maps.
    RunninSnapshotMap                                     snapshot_runinnig_;               ///< Running snapshots keyed by name.
    PVSnapshotMap                                         pv_snapshot_map_;                 ///< Multimap from PV name to snapshot operations.
    std::shared_ptr<BS::light_thread_pool>                thread_pool;                      ///< Worker pool for submission tasks.
    k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager;            ///< EPICS services entry point.
    k2eg::common::BroadcastToken                          epics_handler_token;              ///< Liveness token for EPICS event handlers.
    k2eg::service::metric::INodeControllerMetric&         metrics;                          ///< Node controller metrics sink.
    std::thread                                           expiration_thread;                ///< Background thread that checks for expired snapshots.
    std::atomic<bool>                                     expiration_thread_running{false}; ///< True while the expiration thread is running.
    std::unique_ptr<SnapshotIterationSynchronizer>        iteration_sync_;                  ///< Per-snapshot iteration ordering helper.
    std::unordered_map<std::string, int64_t>              active_iteration_id_;             ///< Active iteration id per snapshot during scheduling.

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
    ContinuousSnapshotManager(const RepeatingSnaptshotConfiguration& repeating_snapshot_configuration, k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager);

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
