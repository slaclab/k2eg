#ifndef K2EG_CONTROLLER_NODE_WORKER_SNAPSHOTCOMMANDWORKER_H_
#define K2EG_CONTROLLER_NODE_WORKER_SNAPSHOTCOMMANDWORKER_H_

#include <k2eg/service/epics/EpicsData.h>
#include <k2eg/service/epics/EpicsMonitorOperation.h>
#include <k2eg/service/epics/EpicsServiceManager.h>
#include <k2eg/service/metric/IMetricService.h>

#include <k2eg/controller/command/cmd/SnapshotCommand.h>
#include <k2eg/controller/node/worker/CommandWorker.h>
#include <k2eg/controller/node/worker/snapshot/ContinuousSnapshotManager.h>

#include <boost/dynamic_bitset.hpp>

#include <stdint.h>
#include <vector>

namespace k2eg::controller::node::worker {

/*
Snapshot reply message
*/
struct SnapshotCommandReply : public k2eg::controller::node::worker::CommandReply
{
    const std::int32_t                                 element_number;
    k2eg::service::epics_impl::ConstChannelDataShrdPtr pv_data;
};
DEFINE_PTR_TYPES(SnapshotCommandReply)

/*
    Snapshot faulty reply message
*/
struct SnapshotFaultyCommandReply : public k2eg::controller::node::worker::CommandReply
{
    const std::string message;
};
DEFINE_PTR_TYPES(SnapshotFaultyCommandReply)

struct ContinuousSnapshotCommandReply : public k2eg::controller::node::worker::CommandReply
{
    const std::string message;
    const std::string publishing_topic;
};
DEFINE_PTR_TYPES(ContinuousSnapshotCommandReply)

/**
Get reply message json serialization
*/
inline void serializeJson(const SnapshotCommandReply& reply, common::JsonMessage& json_message)
{
    serializeJson(static_cast<CommandReply>(reply), json_message);
    service::epics_impl::epics_serializer_factory.resolve(common::SerializationType::JSON)->serialize(*reply.pv_data, json_message);
}

inline void serializeJson(const SnapshotFaultyCommandReply& reply, common::JsonMessage& json_message)
{
    serializeJson(static_cast<CommandReply>(reply), json_message);
    if (!reply.message.empty())
    {
        json_message.getJsonObject()["message"] = reply.message;
    }
}

inline void serializeJson(const ContinuousSnapshotCommandReply& reply, common::JsonMessage& json_message)
{
    serializeJson(static_cast<CommandReply>(reply), json_message);
    if (!reply.message.empty())
    {
        json_message.getJsonObject()["message"] = reply.message;
    }
    if (!reply.publishing_topic.empty())
    {
        json_message.getJsonObject()["publishing_topic"] = reply.publishing_topic;
    }
}

/**
Get reply message msgpack serialization
*/
inline void serializeMsgpack(const SnapshotCommandReply& reply, common::MsgpackMessage& msgpack_message, std::uint8_t map_size = 0)
{
    serializeMsgpack(static_cast<CommandReply>(reply), msgpack_message, map_size + 1);
    service::epics_impl::epics_serializer_factory.resolve(common::SerializationType::Msgpack)->serialize(*reply.pv_data, msgpack_message);
}

inline void serializeMsgpack(const SnapshotFaultyCommandReply& reply, common::MsgpackMessage& msgpack_message, std::uint8_t map_size = 0)
{
    serializeMsgpack(static_cast<CommandReply>(reply), msgpack_message, map_size + (reply.message.empty() ? 0 : 1));
    msgpack::packer<msgpack::sbuffer> packer(msgpack_message.getBuffer());
    if (!reply.message.empty())
    {
        packer.pack("message");
        packer.pack(reply.message);
    }
}

inline void serializeMsgpack(const ContinuousSnapshotCommandReply& reply, common::MsgpackMessage& msgpack_message, std::uint8_t map_size = 0)
{
    // calculate the size of the map
    // 1 for the message and 1 for the publishing topic
    int field_size = (reply.message.empty() ? 0 : 1) + (reply.publishing_topic.empty() ? 0 : 1);
    serializeMsgpack(static_cast<CommandReply>(reply), msgpack_message, map_size + field_size);
    msgpack::packer<msgpack::sbuffer> packer(msgpack_message.getBuffer());
    if (!reply.message.empty())
    {
        packer.pack("message");
        packer.pack(reply.message);
    }
    if (!reply.publishing_topic.empty())
    {
        packer.pack("publishing_topic");
        packer.pack(reply.publishing_topic);
    }
}

/**
Get reply message msgpack compact serialization
*/
inline void serializeMsgpackCompact(const SnapshotCommandReply& reply, common::MsgpackMessage& msgpack_message, std::uint8_t map_size = 0)
{
    serializeMsgpackCompact(static_cast<CommandReply>(reply), msgpack_message, map_size + 1);
    service::epics_impl::epics_serializer_factory.resolve(common::SerializationType::MsgpackCompact)->serialize(*reply.pv_data, msgpack_message);
}

inline void serializeMsgpackCompact(const SnapshotFaultyCommandReply& reply, common::MsgpackMessage& msgpack_message, std::uint8_t map_size = 0)
{
    serializeMsgpackCompact(static_cast<CommandReply>(reply), msgpack_message, map_size + (reply.message.empty() ? 0 : 1));
    msgpack::packer<msgpack::sbuffer> packer(msgpack_message.getBuffer());
    if (!reply.message.empty())
    {
        packer.pack("message");
        packer.pack(reply.message);
    }
}

inline void serializeMsgpackCompact(const ContinuousSnapshotCommandReply& reply, common::MsgpackMessage& msgpack_message, std::uint8_t map_size = 0)
{
    int field_size = (reply.message.empty() ? 0 : 1) + (reply.publishing_topic.empty() ? 0 : 1);
    serializeMsgpackCompact(static_cast<CommandReply>(reply), msgpack_message, map_size + (reply.message.empty() ? 0 : 1));
    msgpack::packer<msgpack::sbuffer> packer(msgpack_message.getBuffer());
    if (!reply.message.empty())
    {
        packer.pack("message");
        packer.pack(reply.message);
    }
    if (!reply.publishing_topic.empty())
    {
        packer.pack("publishing_topic");
        packer.pack(reply.publishing_topic);
    }
}

/*
Is the worker that take care to manage the snapshot command
and collect all the structure for the monitor operation
for all the PV
*/
class SnapshotOpInfo : public WorkerAsyncOperation
{
public:
    // keep track of the comamnd specification
    k2eg::controller::command::cmd::ConstSnapshotCommandShrdPtr cmd;
    // take track for all the monitor operation that have been processed
    boost::dynamic_bitset<> processed_index;
    // contains the monitor async opration for all the PVs
    std::vector<service::epics_impl::ConstMonitorOperationShrdPtr> v_mon_ops;

    SnapshotOpInfo(k2eg::controller::command::cmd::ConstSnapshotCommandShrdPtr cmd, std::vector<service::epics_impl::ConstMonitorOperationShrdPtr> v_mon_ops, std::uint32_t tout_msc = 1000)
        : WorkerAsyncOperation(std::chrono::milliseconds(tout_msc)), processed_index(v_mon_ops.size()), cmd(cmd), v_mon_ops(std::move(v_mon_ops))
    {
    }
};
DEFINE_PTR_TYPES(SnapshotOpInfo)

/**
 * Worker responsible for handling snapshot commands.
 * It manages the lifecycle of the snapshot operation,
 * including data collection and response publication.
 */
class SnapshotCommandWorker : public CommandWorker
{
    k2eg::service::log::ILoggerShrdPtr                    logger;
    k2eg::service::pubsub::IPublisherShrdPtr              publisher;
    k2eg::service::metric::IEpicsMetric&                  metric;
    k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager;
    snapshot::ContinuousSnapshotManager                   continuous_snapshot_manager;
    // Receive event from publisher
    void publishEvtCB(k2eg::service::pubsub::EventType type, k2eg::service::pubsub::PublishMessage* const msg, const std::string& error_message);
    // manage the snapshot command execution in a separate thread
    void checkGetCompletion(std::shared_ptr<BS::light_thread_pool> thread_pool, SnapshotOpInfoShrdPtr snapshot_info);
    // send a faulty reply to the client
    void manageFaultyReply(const std::int8_t error_code, const std::string& error_message, k2eg::controller::command::cmd::ConstSnapshotCommandShrdPtr cmd);
    // send the snapshot reply to the client for a pv index
    /*
    The function return the value of a pv in a specific index respect to the original list of PVs when it is ready
    so the client could receive event in a different order than the original list of PVs and it need to reconsturct the
    original list of PVs
    */
    void publishSnapshotReply(k2eg::controller::command::cmd::ConstSnapshotCommandShrdPtr cmd, std::uint32_t pv_index, service::epics_impl::ConstChannelDataShrdPtr pv_data);
    /*
    Is the final message that is sent to the client to notify that the snapshot has been completed
    */
    void publishEndSnapshotReply(k2eg::controller::command::cmd::ConstSnapshotCommandShrdPtr cmd);
    /*
        preparae and submit the single snapshot command to the thread pool
    */
    void submitSingleSnapshot(std::shared_ptr<BS::light_thread_pool> command_pool, k2eg::controller::command::cmd::ConstCommandShrdPtr command);

public:
    SnapshotCommandWorker(k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager);
    virtual ~SnapshotCommandWorker();
    // process the command
    void processCommand(std::shared_ptr<BS::light_thread_pool> thread_pool, k2eg::controller::command::cmd::ConstCommandShrdPtr command);
    std::size_t getTaskRunning() const;
};

} // namespace k2eg::controller::node::worker
#endif // K2EG_CONTROLLER_NODE_WORKER_SNAPSHOTCOMMANDWORKER_H_
