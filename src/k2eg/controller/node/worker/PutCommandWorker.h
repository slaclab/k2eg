#ifndef K2EG_CONTROLLER_NODE_WORKER_PUTCOMMANDWORKER_H_
#define K2EG_CONTROLLER_NODE_WORKER_PUTCOMMANDWORKER_H_

#include <k2eg/common/types.h>
#include <k2eg/controller/node/worker/CommandWorker.h>
#include <k2eg/service/epics/EpicsServiceManager.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/pubsub/IPublisher.h>
#include <k2eg/service/metric/IMetricService.h>

#include <k2eg/common/BS_thread_pool.hpp>
#include <string>

#include "k2eg/service/epics/EpicsPutOperation.h"

namespace k2eg::controller::node::worker {

/**
Put reply message
*/
struct PutCommandReply : public k2eg::controller::node::worker::CommandReply {
  //k2eg::service::epics_impl::ConstChannelDataUPtr pv_data;
};
DEFINE_PTR_TYPES(PutCommandReply)

/**
Put reply message json serialization
*/
inline void
serializeJson(const PutCommandReply& reply, common::JsonMessage& json_message) {
  serializeJson(static_cast<CommandReply>(reply), json_message);
  //service::epics_impl::epics_serializer_factory.resolve(common::SerializationType::JSON)->serialize(*reply.pv_data, json_message);
}

/**
Put reply message msgpack serialization
*/
inline void
serializeMsgpack(const PutCommandReply& reply, common::MsgpackMessage& msgpack_message, std::uint8_t map_size = 0) {
  serializeMsgpack(static_cast<CommandReply>(reply), msgpack_message, map_size);
  //service::epics_impl::epics_serializer_factory.resolve(common::SerializationType::Msgpack)->serialize(*reply.pv_data, msgpack_message);
}

/**
Put reply message msgpack compact serialization
*/
inline void
serializeMsgpackCompact(const PutCommandReply& reply, common::MsgpackMessage& msgpack_message, std::uint8_t map_size = 0) {
  serializeMsgpackCompact(static_cast<CommandReply>(reply), msgpack_message, map_size);
  //service::epics_impl::epics_serializer_factory.resolve(common::SerializationType::MsgpackCompact)->serialize(*reply.pv_data, msgpack_message);
}

struct PutOpInfo : public WorkerAsyncOperation {
  std::string                                pv_name;
  std::string                                destination_topic;
  std::string                                value;
  std::string                                reply_id;
  k2eg::common::SerializationType            serialization;
  service::epics_impl::ConstPutOperationUPtr op;
  PutOpInfo(
    const std::string& pv_name, 
    const std::string& destination_topic, 
    const std::string& value, 
    const std::string& reply_id, 
    const k2eg::common::SerializationType&     serialization,
    service::epics_impl::ConstPutOperationUPtr op,
    std::uint32_t tout_msc = 10000)
      : WorkerAsyncOperation(std::chrono::milliseconds(tout_msc))
      , destination_topic(destination_topic)
      , pv_name(pv_name)
      , value(value)
      , reply_id(reply_id)
      , serialization(serialization)
      , op(std::move(op)) {}
};
DEFINE_PTR_TYPES(PutOpInfo)

// Class that implements the put epics command
class PutCommandWorker : public CommandWorker {
  std::shared_ptr<BS::thread_pool>                      processing_pool;
  k2eg::service::log::ILoggerShrdPtr                    logger;
  k2eg::service::pubsub::IPublisherShrdPtr              publisher;
  k2eg::service::metric::IEpicsMetric&                  metric;
  k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager;
  void                                                  checkPutCompletion(PutOpInfoShrdPtr put_info);
  k2eg::common::ConstSerializedMessageShrdPtr           getReply(PutOpInfoShrdPtr put_info);
 public:
  PutCommandWorker(k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager);
  virtual ~PutCommandWorker();
  void processCommand(k2eg::controller::command::cmd::ConstCommandShrdPtr command);
};
}  // namespace k2eg::controller::node::worker

#endif  // K2EG_CONTROLLER_NODE_WORKER_PUTCOMMANDWORKER_H_