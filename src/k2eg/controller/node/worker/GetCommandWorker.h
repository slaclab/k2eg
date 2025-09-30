#ifndef k2eg_CONTROLLER_NODE_WORKER_GETCOMMANDWORKER_H_
#define k2eg_CONTROLLER_NODE_WORKER_GETCOMMANDWORKER_H_

#include <k2eg/common/BS_thread_pool.hpp>
#include <k2eg/common/BaseSerialization.h>
#include <k2eg/common/types.h>

#include <k2eg/service/epics/EpicsGetOperation.h>
#include <k2eg/service/epics/EpicsServiceManager.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/metric/IMetricService.h>
#include <k2eg/service/pubsub/IPublisher.h>

#include <k2eg/controller/command/cmd/Command.h>
#include <k2eg/controller/node/worker/CommandWorker.h>

#include <chrono>
#include <string>

namespace k2eg::controller::node::worker {

/**
 * @brief Reply for a successful GET command.
 *
 * Carries the EPICS channel data returned by the get operation.
 */
struct GetCommandReply : public k2eg::controller::node::worker::CommandReply
{
    k2eg::service::epics_impl::ConstChannelDataUPtr pv_data;
};
DEFINE_PTR_TYPES(GetCommandReply)

/**
 * @brief Reply for a failed GET command.
 *
 * Provides a human-readable error message when a GET cannot be fulfilled.
 */
struct GetFaultyCommandReply : public k2eg::controller::node::worker::CommandReply
{
    const std::string message;
};
DEFINE_PTR_TYPES(GetFaultyCommandReply)

/**
 * @brief Serialize a GET reply to JSON.
 * @param reply source reply to serialize
 * @param json_message destination JSON message wrapper
 */
inline void
serializeJson(const GetCommandReply& reply, common::JsonMessage& json_message)
{
    serializeJson(static_cast<CommandReply>(reply), json_message);
    service::epics_impl::epics_serializer_factory.resolve(common::SerializationType::JSON)->serialize(*reply.pv_data, json_message);
}

inline void
serializeJson(const GetFaultyCommandReply& reply, common::JsonMessage& json_message)
{
    serializeJson(static_cast<CommandReply>(reply), json_message);
    if (!reply.message.empty())
    {
        json_message.getJsonObject()["message"] = reply.message;
    }
}

/**
 * @brief Serialize a GET reply to Msgpack.
 * @param reply source reply to serialize
 * @param msgpack_message destination Msgpack wrapper
 * @param map_size extra map entries contributed by callers (default 0)
 */
inline void
serializeMsgpack(const GetCommandReply& reply, common::MsgpackMessage& msgpack_message, std::uint8_t map_size = 0)
{
    serializeMsgpack(static_cast<CommandReply>(reply), msgpack_message, map_size + 1);
    service::epics_impl::epics_serializer_factory.resolve(common::SerializationType::Msgpack)->serialize(*reply.pv_data, msgpack_message);
}

inline void
serializeMsgpack(const GetFaultyCommandReply& reply, common::MsgpackMessage& msgpack_message, std::uint8_t map_size = 0)
{
    serializeMsgpack(static_cast<CommandReply>(reply), msgpack_message, map_size + (reply.message.empty() ? 0 : 1));
    msgpack::packer<msgpack::sbuffer> packer(msgpack_message.getBuffer());
    if (!reply.message.empty())
    {
        packer.pack("message");
        packer.pack(reply.message);
    }
}

/**
 * @brief Serialize a GET reply to compact Msgpack.
 * @param reply source reply to serialize
 * @param msgpack_message destination Msgpack wrapper
 * @param map_size extra map entries contributed by callers (default 0)
 */
inline void
serializeMsgpackCompact(const GetCommandReply& reply, common::MsgpackMessage& msgpack_message, std::uint8_t map_size = 0)
{
    serializeMsgpackCompact(static_cast<CommandReply>(reply), msgpack_message, map_size + 1);
    service::epics_impl::epics_serializer_factory.resolve(common::SerializationType::MsgpackCompact)->serialize(*reply.pv_data, msgpack_message);
}

inline void
serializeMsgpackCompact(const GetFaultyCommandReply& reply, common::MsgpackMessage& msgpack_message, std::uint8_t map_size = 0)
{
    serializeMsgpackCompact(static_cast<CommandReply>(reply), msgpack_message, map_size + (reply.message.empty() ? 0 : 1));
    msgpack::packer<msgpack::sbuffer> packer(msgpack_message.getBuffer());
    if (!reply.message.empty())
    {
        packer.pack("message");
        packer.pack(reply.message);
    }
}

/**
 * @brief Async operation metadata for a single GET.
 *
 * Holds the original command, the EPICS get operation handle, and a timeout.
 */
class GetOpInfo : public WorkerAsyncOperation
{
public:
    k2eg::controller::command::cmd::ConstGetCommandShrdPtr cmd;
    service::epics_impl::ConstGetOperationUPtr             op;

    /**
     * @brief Construct a new GET operation info.
     * @param cmd originating GET command
     * @param op EPICS get operation handle (unique ownership)
     * @param tout_msc timeout in milliseconds (default 10000)
     */
    GetOpInfo(k2eg::controller::command::cmd::ConstGetCommandShrdPtr cmd, k2eg::service::epics_impl::ConstGetOperationUPtr op, std::uint32_t tout_msc = 10000)
        : WorkerAsyncOperation(std::chrono::milliseconds(tout_msc)), cmd(cmd), op(std::move(op)) {}
};
DEFINE_PTR_TYPES(GetOpInfo)

/**
 * @brief Worker that processes EPICS GET commands.
 */
class GetCommandWorker : public CommandWorker
{
    k2eg::service::log::ILoggerShrdPtr                    logger;
    k2eg::service::pubsub::IPublisherShrdPtr              publisher;
    k2eg::service::metric::IEpicsMetric&                  metric;
    k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager;
    void                                                  publishEvtCB(k2eg::service::pubsub::EventType type, k2eg::service::pubsub::PublishMessage* const msg, const std::string& error_message);
    void                                                  checkGetCompletion(std::shared_ptr<BS::light_thread_pool> command_pool, GetOpInfoShrdPtr put_info);
    void                                                  manageFaultyReply(const std::int8_t error_code, const std::string& error_message, k2eg::controller::command::cmd::ConstGetCommandShrdPtr cmd);

public:
    /**
     * @brief Create a GET worker bound to an EPICS service manager.
     * @param epics_service_manager EPICS service manager used to perform operations
     */
    GetCommandWorker(k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager);
    /**
     * @brief Destroy the GET worker.
     */
    virtual ~GetCommandWorker();
    /**
     * @brief Process a single command if it is a GET.
     * @param command_pool worker pool to offload async checks
     * @param command incoming command (expects GET subtype)
     */
    void processCommand(std::shared_ptr<BS::light_thread_pool> command_pool, k2eg::controller::command::cmd::ConstCommandShrdPtr command);
};

} // namespace k2eg::controller::node::worker

#endif // k2eg_CONTROLLER_NODE_WORKER_GETCOMMANDWORKER_H_
