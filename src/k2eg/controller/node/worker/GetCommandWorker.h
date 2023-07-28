#ifndef k2eg_CONTROLLER_NODE_WORKER_GETCOMMANDWORKER_H_
#define k2eg_CONTROLLER_NODE_WORKER_GETCOMMANDWORKER_H_

#include <k2eg/common/BaseSerialization.h>
#include <k2eg/common/types.h>
#include <k2eg/controller/command/cmd/Command.h>
#include <k2eg/controller/node/worker/CommandWorker.h>
#include <k2eg/service/epics/EpicsGetOperation.h>
#include <k2eg/service/epics/EpicsServiceManager.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/metric/IMetricService.h>
#include <k2eg/service/pubsub/IPublisher.h>

#include <chrono>
#include <k2eg/common/BS_thread_pool.hpp>
#include <string>

namespace k2eg::controller::node::worker {

/**
Get reply message
*/
struct GetCommandReply : public k2eg::controller::node::worker::CommandReply {
  k2eg::service::epics_impl::ConstChannelDataUPtr pv_data;
};
DEFINE_PTR_TYPES(GetCommandReply)

struct GetFaultyCommandReply : public k2eg::controller::node::worker::CommandReply {
  const std::string message;
};
DEFINE_PTR_TYPES(GetFaultyCommandReply)

/**
Get reply message json serialization
*/
inline void
serializeJson(const GetCommandReply& reply, common::JsonMessage& json_message) {
  serializeJson(static_cast<CommandReply>(reply), json_message);
  service::epics_impl::epics_serializer_factory.resolve(common::SerializationType::JSON)->serialize(*reply.pv_data, json_message);
}
inline void
serializeJson(const GetFaultyCommandReply& reply, common::JsonMessage& json_message) {
  serializeJson(static_cast<CommandReply>(reply), json_message);
  if (!reply.message.empty()) { json_message.getJsonObject()["message"] = reply.message; }
}
/**
Get reply message msgpack serialization
*/
inline void
serializeMsgpack(const GetCommandReply& reply, common::MsgpackMessage& msgpack_message, std::uint8_t map_size = 0) {
  serializeMsgpack(static_cast<CommandReply>(reply), msgpack_message, map_size + 1);
  service::epics_impl::epics_serializer_factory.resolve(common::SerializationType::Msgpack)->serialize(*reply.pv_data, msgpack_message);
}
inline void
serializeMsgpack(const GetFaultyCommandReply& reply, common::MsgpackMessage& msgpack_message, std::uint8_t map_size = 0) {
  serializeMsgpack(static_cast<CommandReply>(reply), msgpack_message, map_size + (reply.message.empty() ? 0 : 1));
  msgpack::packer<msgpack::sbuffer> packer(msgpack_message.getBuffer());
  if (!reply.message.empty()) {
    packer.pack("message");
    packer.pack(reply.message);
  }
}
/**
Get reply message msgpack compact serialization
*/
inline void
serializeMsgpackCompact(const GetCommandReply& reply, common::MsgpackMessage& msgpack_message, std::uint8_t map_size = 0) {
  serializeMsgpackCompact(static_cast<CommandReply>(reply), msgpack_message, map_size + 1);
  service::epics_impl::epics_serializer_factory.resolve(common::SerializationType::MsgpackCompact)->serialize(*reply.pv_data, msgpack_message);
}
inline void
serializeMsgpackCompact(const GetFaultyCommandReply& reply, common::MsgpackMessage& msgpack_message, std::uint8_t map_size = 0) {
  serializeMsgpackCompact(static_cast<CommandReply>(reply), msgpack_message, map_size + (reply.message.empty() ? 0 : 1));
  msgpack::packer<msgpack::sbuffer> packer(msgpack_message.getBuffer());
  if (!reply.message.empty()) {
    packer.pack("message");
    packer.pack(reply.message);
  }
}

/**
 */
class GetOpInfo : public WorkerAsyncOperation {
 public:
  k2eg::controller::command::cmd::ConstGetCommandShrdPtr cmd;
  service::epics_impl::ConstGetOperationUPtr             op;
  GetOpInfo(k2eg::controller::command::cmd::ConstGetCommandShrdPtr cmd, k2eg::service::epics_impl::ConstGetOperationUPtr op, std::uint32_t tout_msc = 10000)
      : WorkerAsyncOperation(std::chrono::milliseconds(tout_msc)), cmd(cmd), op(std::move(op)) {}
};
DEFINE_PTR_TYPES(GetOpInfo)

class GetCommandWorker : public CommandWorker {
  std::shared_ptr<BS::thread_pool>                      processing_pool;
  k2eg::service::log::ILoggerShrdPtr                    logger;
  k2eg::service::pubsub::IPublisherShrdPtr              publisher;
  k2eg::service::metric::IEpicsMetric&                  metric;
  k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager;
  void                                                  checkGetCompletion(GetOpInfoShrdPtr put_info);
  void                                                  manageFaultyReply(const std::int8_t error_code, const std::string& error_message, k2eg::controller::command::cmd::ConstGetCommandShrdPtr cmd);
 public:
  GetCommandWorker(k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager);
  virtual ~GetCommandWorker();
  void processCommand(k2eg::controller::command::cmd::ConstCommandShrdPtr command);
};

}  // namespace k2eg::controller::node::worker

#endif  // k2eg_CONTROLLER_NODE_WORKER_GETCOMMANDWORKER_H_