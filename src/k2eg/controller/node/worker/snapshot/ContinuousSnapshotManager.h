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
#include <k2eg/controller/node/worker/SnapshotCommandWorker.h>
#include <k2eg/controller/node/worker/snapshot/SnapshotOpInfo.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <string>
#include <cstddef>
#include <unordered_map>

namespace k2eg::controller::node::worker::snapshot {
#pragma region Types

struct RepeatingSnaptshotConfiguration
{
    // the cron stirng for schedule the monitor
    size_t snapshot_processing_thread_count = 1;
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
    const RepeatingSnaptshotConfiguration& repeating_snapshot_configuration;
    // define the run flag
    std::atomic<bool> run_flag = false;
    // local logger shared instances
    k2eg::service::log::ILoggerShrdPtr logger;
    // local publisher shared instance
    k2eg::service::pubsub::IPublisherShrdPtr publisher;
    
    // keep track of running snapshots
    mutable std::shared_mutex snapshot_runinnig_mutex_;
    RunninSnapshotMap snapshot_runinnig_;
    PVSnapshotMap pv_snapshot_map_;

    // thread pool for snapshot processing
    std::shared_ptr<BS::light_thread_pool> thread_pool;
    // EPICS service manager
    k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager;

    // Handler's liveness token
    k2eg::common::BroadcastToken epics_handler_token;

    // metric statistic
    k2eg::service::metric::INodeControllerMetric& metrics;
    // vector for the throttlig statistic for each thread
    std::vector<k2eg::common::ThrottlingManagerUPtr> thread_throttling_vector;

    // Received event from EPICS IOCs that are monitored
    void epicsMonitorEvent(k2eg::service::epics_impl::EpicsServiceManagerHandlerParamterType event_received);
    // rpocess each snapshot checking if the timewindopws is epxired tahing the data from the cache
    // and publishing the data
    void processSnapshot(SnapshotOpInfoShrdPtr snapshot_command_info);
    // Manager the reply to the client durin gthe snapshto submission
    void manageReply(const std::int8_t error_code, const std::string& error_message, k2eg::controller::command::cmd::ConstCommandShrdPtr command, const std::string& publishing_topic = "");
    // is the callback for the publisher
    void publishEvtCB(k2eg::service::pubsub::EventType type, k2eg::service::pubsub::PublishMessage* const msg, const std::string& error_message);
    // manage the start of the snapshot
    void startSnapshot(command::cmd::ConstRepeatingSnapshotCommandShrdPtr command);
    // manage the trigger of the snapshot
    void triggerSnapshot(command::cmd::ConstRepeatingSnapshotTriggerCommandShrdPtr command);
    // manage the stop of the snapshot
    void stopSnapshot(command::cmd::ConstRepeatingSnapshotStopCommandShrdPtr command);
    // manage the statistic collection of the snapshots engine
    void handleStatistic(k2eg::service::scheduler::TaskProperties& task_properties);

public:
    ContinuousSnapshotManager(const RepeatingSnaptshotConfiguration& repeating_snapshot_configuration, k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager);
    ~ContinuousSnapshotManager();
    void        submitSnapshot(k2eg::controller::command::cmd::ConstCommandShrdPtr snapsthot_command);
    std::size_t getRunningSnapshotCount() const;
};

DEFINE_PTR_TYPES(ContinuousSnapshotManager)
} // namespace k2eg::controller::node::worker::snapshot

#endif // K2EG_CONTROLLER_NODE_WORKER_MONITOR_CONTINUOUSSNAPSHOTMANAGER_H_