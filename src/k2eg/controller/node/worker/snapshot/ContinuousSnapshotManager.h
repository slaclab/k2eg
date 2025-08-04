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

struct RepeatingSnaptshotConfiguration
{
    // the cron stirng for schedule the monitor
    size_t snapshot_processing_thread_count = 1;

    const std::string toString() const
    {
        return std::format("RepeatingSnaptshotConfiguration(snapshot_processing_thread_count={})", snapshot_processing_thread_count);
    }
};
DEFINE_PTR_TYPES(RepeatingSnaptshotConfiguration)

/*
    @brief Repeating snapshot message header
    @details This message is used to send the header of a repeating snapshot reply.
    the messages that belong to this snapshot are sent in the same topic sequentially
*/
struct RepeatingSnaptshotHeader
{
    const std::int8_t message_type = 0;
    // this is the snapshot name
    const std::string snapshot_name;
    // this is the snapshot timestamp
    const std::int64_t timestamp;
    // this is the snapshot iteration
    const std::int64_t iteration_index;
};
DEFINE_PTR_TYPES(RepeatingSnaptshotHeader)

#pragma region Serialization

/*
    @brief Repeating snapshot data
    @details This message is used to send the data for a specific snapshot and pv
*/
struct RepeatingSnaptshotData
{
    const std::int8_t message_type = 1;
    // this is the snapshot instance where the pv is related
    const std::int64_t timestamp;
    // this is the snapshot iteration
    const std::int64_t iteration_index;
    // this is the snapshot values for a specific pv
    k2eg::service::epics_impl::ConstChannelDataShrdPtr pv_data;
};
DEFINE_PTR_TYPES(RepeatingSnaptshotData)

/*
@brief Repeating snapshot completion
@details This message is used to send the completion of a current submission
*/
struct RepeatingSnaptshotCompletion
{
    const std::int8_t message_type = 2;
    // this is the snapshot error code
    const std::int32_t error;
    // this is the snapshot error message( in case there is one)
    const std::string error_message;
    // this is the snapshot name
    const std::string snapshot_name;
    // this is the snapshot instance where the pv is related
    const std::int64_t timestamp;
    // this is the snapshot iteration
    const std::int64_t iteration_index;
};
DEFINE_PTR_TYPES(RepeatingSnaptshotCompletion)

/*
json serialization for the repeating snapshot header and data
*/
inline void serializeJson(const RepeatingSnaptshotHeader& event_header, common::JsonMessage& json_message)
{
    json_message.getJsonObject()["message_type"] = event_header.message_type;
    json_message.getJsonObject()["iter_index"] = event_header.iteration_index;
    json_message.getJsonObject()["snapshot_name"] = event_header.snapshot_name;
    json_message.getJsonObject()["timestamp"] = event_header.timestamp;
}

inline void serializeJson(const RepeatingSnaptshotData& event_data, common::JsonMessage& json_message)
{
    json_message.getJsonObject()["message_type"] = event_data.message_type;
    json_message.getJsonObject()["iter_index"] = event_data.iteration_index;
    json_message.getJsonObject()["timestamp"] = event_data.timestamp;
    service::epics_impl::epics_serializer_factory.resolve(common::SerializationType::JSON)->serialize(*event_data.pv_data, json_message);
}

inline void serializeJson(const RepeatingSnaptshotCompletion& event_completion, common::JsonMessage& json_message)
{
    json_message.getJsonObject()["message_type"] = event_completion.message_type;
    json_message.getJsonObject()["error"] = event_completion.error;
    json_message.getJsonObject()["error_message"] = event_completion.error_message;
    json_message.getJsonObject()["iter_index"] = event_completion.iteration_index;
    json_message.getJsonObject()["timestamp"] = event_completion.timestamp;
    json_message.getJsonObject()["snapshot_name"] = event_completion.snapshot_name;
}

/*
msgpack serialization for the repeating snapshot header and data
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

inline void serializeMsgpack(const RepeatingSnaptshotData& data_event, common::MsgpackMessage& msgpack_message, std::uint8_t map_size = 0)
{
    msgpack::packer<msgpack::sbuffer> packer(msgpack_message.getBuffer());
    packer.pack_map(4);
    packer.pack("message_type");
    packer.pack(data_event.message_type);
    packer.pack("timestamp");
    packer.pack(data_event.timestamp);
    packer.pack("iter_index");
    packer.pack(data_event.iteration_index);
    service::epics_impl::epics_serializer_factory.resolve(common::SerializationType::Msgpack)->serialize(*data_event.pv_data, msgpack_message);
}

inline void serializeMsgpack(const RepeatingSnaptshotCompletion& header_completion, common::MsgpackMessage& msgpack_message, std::uint8_t map_size = 0)
{
    msgpack::packer<msgpack::sbuffer> packer(msgpack_message.getBuffer());
    packer.pack_map(header_completion.error_message.empty() ? 5 : 6);
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
    packer.pack("snapshot_name");
    packer.pack(header_completion.snapshot_name);
}

// global serialization function
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
    std::atomic<int>  active_tasks{0};
    std::atomic<bool> tail_processed{false};
};

class SnapshotIterationSynchronizer
{
private:
    mutable std::shared_mutex                                                                      iteration_mutex_;
    std::unordered_map<std::string, uint64_t>                                                      current_iteration_;
    std::unordered_map<std::string, std::atomic<bool>>                                             iteration_in_progress_;
    std::unordered_map<std::string, std::unique_ptr<std::condition_variable_any>>                  iteration_cv_;
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
    bool                                     last_submition = false; // flag to indicate if the task should close
    std::shared_ptr<SnapshotOpInfo>          snapshot_command_info;  // shared pointer to the snapshot operation info
    SnapshotSubmissionShrdPtr                submission_shrd_ptr;
    k2eg::service::pubsub::IPublisherShrdPtr publisher;
    k2eg::service::log::ILoggerShrdPtr       logger;
    SnapshotIterationSynchronizer&           iteration_sync_;

public:
    SnapshotSubmissionTask(std::shared_ptr<SnapshotOpInfo> snapshot_command_info, SnapshotSubmissionShrdPtr submission_shrd_ptr, k2eg::service::pubsub::IPublisherShrdPtr publisher, k2eg::service::log::ILoggerShrdPtr logger, SnapshotIterationSynchronizer& iteration_sync, bool last_submition = false);

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
    const RepeatingSnaptshotConfiguration& repeating_snapshot_configuration;

    // Flag indicating if the manager is running
    std::atomic<bool> run_flag = false;

    // Shared pointer to the logger instance
    k2eg::service::log::ILoggerShrdPtr logger;

    // Node cofiguration for accessing snapshot specific settings
    service::configuration::INodeConfigurationShrdPtr node_configuration;

    // Shared pointer to the publisher instance for publishing snapshot data
    k2eg::service::pubsub::IPublisherShrdPtr publisher;

    // Mutex for synchronizing access to running snapshots
    mutable std::shared_mutex snapshot_runinnig_mutex_;

    // Map of currently running snapshots (snapshot name -> SnapshotOpInfo)
    RunninSnapshotMap snapshot_runinnig_;

    // Multimap of PV names to their associated snapshot operations
    // mutliple snapshots can be associated with the same PV
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