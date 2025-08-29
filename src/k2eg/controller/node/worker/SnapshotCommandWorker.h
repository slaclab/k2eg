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

/**
 * @brief Configuration for snapshot command handling.
 */
struct SnapshotCommandConfiguration
{
    /** @brief Parameters for repeating/continuous snapshot engine. */
    snapshot::RepeatingSnaptshotConfiguration continuous_snapshot_configuration;
};
DEFINE_PTR_TYPES(SnapshotCommandConfiguration)

/**
 * @brief Reply payload for a snapshot command containing PV data.
 */
struct SnapshotCommandReply : public k2eg::controller::node::worker::CommandReply
{
    const std::int32_t                                 element_number;
    k2eg::service::epics_impl::ConstChannelDataShrdPtr pv_data;
};
DEFINE_PTR_TYPES(SnapshotCommandReply)

/**
 * @brief Faulty reply payload for snapshot commands.
 */
struct SnapshotFaultyCommandReply : public k2eg::controller::node::worker::CommandReply
{
    const std::string message;
};
DEFINE_PTR_TYPES(SnapshotFaultyCommandReply)

/**
 * @brief Reply for continuous snapshot commands (start/stop/trigger).
 */
struct ContinuousSnapshotCommandReply : public k2eg::controller::node::worker::CommandReply
{
    const std::string message;
    const std::string publishing_topic;
};
DEFINE_PTR_TYPES(ContinuousSnapshotCommandReply)

/**
 * @brief Serialize SnapshotCommandReply to JSON.
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
 * @brief Serialize snapshot replies to Msgpack.
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
 * @brief Serialize snapshot replies to compact Msgpack.
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

/**
 * @brief Single-shot snapshot execution context.
 *
 * Manages the monitor operations required to complete a one-off snapshot
 * over a set of PVs and coordinate their completion.
 */
class SnapshotOpInfo : public WorkerAsyncOperation
{
public:
    /** @brief Original snapshot command specification. */
    k2eg::controller::command::cmd::ConstSnapshotCommandShrdPtr cmd;
    /** @brief Bitset tracking processed monitor operations. */
    boost::dynamic_bitset<> processed_index;
    /** @brief Async monitor operations for all PVs. */
    std::vector<service::epics_impl::ConstMonitorOperationShrdPtr> v_mon_ops;

    SnapshotOpInfo(k2eg::controller::command::cmd::ConstSnapshotCommandShrdPtr cmd, std::vector<service::epics_impl::ConstMonitorOperationShrdPtr> v_mon_ops, std::uint32_t tout_msc = 1000)
        : WorkerAsyncOperation(std::chrono::milliseconds(tout_msc)), processed_index(v_mon_ops.size()), cmd(cmd), v_mon_ops(std::move(v_mon_ops))
    {
    }
};
DEFINE_PTR_TYPES(SnapshotOpInfo)

/**
 * @brief Worker that handles snapshot commands (single-shot and continuous).
 *
 * Coordinates EPICS operations, publishes replies, and updates metrics.
 */
class SnapshotCommandWorker : public CommandWorker
{
    const SnapshotCommandConfiguration&                   snapshot_command_configuration;
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
    /**
     * @brief Publish the snapshot reply for a PV index.
     *
     * Replies may arrive out of order relative to the original PV list; the
     * client must reassemble based on index.
     */
    void publishSnapshotReply(k2eg::controller::command::cmd::ConstSnapshotCommandShrdPtr cmd, std::uint32_t pv_index, service::epics_impl::ConstChannelDataShrdPtr pv_data);
    /**
     * @brief Publish the final message indicating snapshot completion.
     */
    void publishEndSnapshotReply(k2eg::controller::command::cmd::ConstSnapshotCommandShrdPtr cmd);
    /**
     * @brief Prepare and submit a single-shot snapshot to the thread pool.
     */
    void submitSingleSnapshot(std::shared_ptr<BS::light_thread_pool> command_pool, k2eg::controller::command::cmd::ConstCommandShrdPtr command);

public:
    SnapshotCommandWorker(const SnapshotCommandConfiguration& snapshot_command_configuration, k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager);
    virtual ~SnapshotCommandWorker();
    /** @brief Process snapshot-related commands. */
    void processCommand(std::shared_ptr<BS::light_thread_pool> thread_pool, k2eg::controller::command::cmd::ConstCommandShrdPtr command);
    std::size_t getTaskRunning() const;
};

} // namespace k2eg::controller::node::worker
#endif // K2EG_CONTROLLER_NODE_WORKER_SNAPSHOTCOMMANDWORKER_H_
