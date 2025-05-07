#ifndef K2EG_CONTROLLER_NODE_WORKER_PUTCOMMANDWORKER_H_
#define K2EG_CONTROLLER_NODE_WORKER_PUTCOMMANDWORKER_H_

#include <k2eg/common/types.h>
#include <k2eg/common/BS_thread_pool.hpp>

#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/pubsub/IPublisher.h>
#include <k2eg/service/metric/IMetricService.h>
#include <k2eg/service/epics/EpicsPutOperation.h>
#include <k2eg/service/epics/EpicsServiceManager.h>

#include <k2eg/controller/node/worker/CommandWorker.h>

#include <msgpack/v3/pack_decl.hpp>

#include <string>

namespace k2eg::controller::node::worker {

/**
Put reply message
*/
struct PutCommandReply : public k2eg::controller::node::worker::CommandReply {
  const std::string message;
  // k2eg::service::epics_impl::ConstChannelDataUPtr pv_data;
};
DEFINE_PTR_TYPES(PutCommandReply)

/**
Put reply message json serialization
*/
inline void
serializeJson(const PutCommandReply& reply, common::JsonMessage& json_message) {
  serializeJson(static_cast<CommandReply>(reply), json_message);
  if (!reply.message.empty()) { json_message.getJsonObject()["message"] = reply.message; }
}

/**
Put reply message msgpack serialization
*/
inline void
serializeMsgpack(const PutCommandReply& reply, common::MsgpackMessage& msgpack_message, std::uint8_t map_size = 0) {
  serializeMsgpack(static_cast<CommandReply>(reply), msgpack_message, map_size + (reply.message.empty() ? 0 : 1));
  msgpack::packer<msgpack::sbuffer> packer(msgpack_message.getBuffer());
  if (!reply.message.empty()) {
    packer.pack("message");
    packer.pack(reply.message);
  }
  // service::epics_impl::epics_serializer_factory.resolve(common::SerializationType::Msgpack)->serialize(*reply.pv_data, msgpack_message);
}

/**
Put reply message msgpack compact serialization
*/
inline void
serializeMsgpackCompact(const PutCommandReply& reply, common::MsgpackMessage& msgpack_message, std::uint8_t map_size = 0) {
  serializeMsgpackCompact(static_cast<CommandReply>(reply), msgpack_message, map_size + (reply.message.empty() ? 0 : 1));
  msgpack::packer<msgpack::sbuffer> packer(msgpack_message.getBuffer());
  if (!reply.message.empty()) {
    packer.pack("message");
    packer.pack(reply.message);
  }
  // service::epics_impl::epics_serializer_factory.resolve(common::SerializationType::MsgpackCompact)->serialize(*reply.pv_data, msgpack_message);
}

struct PutOpInfo : public WorkerAsyncOperation {
  k2eg::controller::command::cmd::ConstPutCommandShrdPtr cmd;
  service::epics_impl::ConstPutOperationUPtr          op;
  PutOpInfo(k2eg::controller::command::cmd::ConstPutCommandShrdPtr cmd, service::epics_impl::ConstPutOperationUPtr op, std::uint32_t tout_msc = 10000)
      : WorkerAsyncOperation(std::chrono::milliseconds(tout_msc)), cmd(cmd), op(std::move(op)) {}
};
DEFINE_PTR_TYPES(PutOpInfo)

// Class that implements the put epics command
class PutCommandWorker : public CommandWorker {
  k2eg::service::log::ILoggerShrdPtr                    logger;
  k2eg::service::pubsub::IPublisherShrdPtr              publisher;
  k2eg::service::metric::IEpicsMetric&                  metric;
  k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager;
  void                                                  checkPutCompletion(std::shared_ptr<BS::light_thread_pool> command_pool, PutOpInfoShrdPtr put_info);
  k2eg::common::ConstSerializedMessageShrdPtr           getReply(PutOpInfoShrdPtr put_info);
  void                                                  manageReply(const std::int8_t error_code, const std::string& error_message, k2eg::controller::command::cmd::ConstPutCommandShrdPtr cmd);

 public:
  PutCommandWorker(k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager);
  virtual ~PutCommandWorker();
  void processCommand(std::shared_ptr<BS::light_thread_pool> command_pool, k2eg::controller::command::cmd::ConstCommandShrdPtr command);
};
}  // namespace k2eg::controller::node::worker

#endif  // K2EG_CONTROLLER_NODE_WORKER_PUTCOMMANDWORKER_H_