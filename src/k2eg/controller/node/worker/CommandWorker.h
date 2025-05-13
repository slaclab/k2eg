#ifndef k2eg_CONTROLLER_NODE_WORKER_COMMANDWORKER_H_
#define k2eg_CONTROLLER_NODE_WORKER_COMMANDWORKER_H_

#include <k2eg/common/types.h>
#include <k2eg/common/serialization.h>
#include <k2eg/common/BS_thread_pool.hpp>
#include <k2eg/common/JsonSerialization.h>
#include <k2eg/common/MsgpackSerialization.h>

#include <k2eg/service/pubsub/IPublisher.h>

#include <k2eg/controller/command/CMDCommand.h>
#include <k2eg/controller/command/cmd/Command.h>

#include <msgpack/v3/pack_decl.hpp>

#include <chrono>
#include <cstdint>
#include <memory>

namespace k2eg::controller::node::worker {

/**
Base command reply structure

Some command cand send a reply as result of operation,
thi class represent the base information for a reply
*/
struct CommandReply {
  //[mandatory] si the error code of the operation done by the command
  const std::int8_t error;
  //[mandatory] is the request id found on the command that has generated the reply
  const std::string reply_id;
};

static void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, CommandReply const& reply) {
  jv = {{"error", reply.error}, {KEY_REPLY_ID, reply.reply_id}};
}

inline void
serializeJson(const CommandReply& reply, common::JsonMessage& json_message) {
  json_message.getJsonObject()["error"] = reply.error;
  json_message.getJsonObject()[KEY_REPLY_ID] = reply.reply_id;
}
inline void
serializeMsgpack(const CommandReply& reply, common::MsgpackMessage& msgpack_message, std::uint8_t map_size = 0) {
  msgpack::packer<msgpack::sbuffer> packer(msgpack_message.getBuffer());
  packer.pack_map(map_size + 2);
  packer.pack("error");
  packer.pack(reply.error);
  packer.pack(KEY_REPLY_ID);
  packer.pack(reply.reply_id);
}
inline void
serializeMsgpackCompact(const CommandReply& reply, common::MsgpackMessage& msgpack_message, std::uint8_t map_size = 0) {
  msgpack::packer<msgpack::sbuffer> packer(msgpack_message.getBuffer());
  packer.pack_map(map_size + 2);
  packer.pack("error");
  packer.pack(reply.error);
  packer.pack(KEY_REPLY_ID);
  packer.pack(reply.reply_id);
}

template <class ReplyObjectClass>
inline common::SerializedMessageShrdPtr
serialize(const ReplyObjectClass& reply, k2eg::common::SerializationType ser_type) {
  common::SerializedMessageShrdPtr result;
  switch (ser_type) {
    case common::SerializationType::Unknown: return common::SerializedMessageShrdPtr();
    case common::SerializationType::JSON: {
      result = std::make_shared<common::JsonMessage>();
      serializeJson(reply, *static_pointer_cast<common::JsonMessage>(result));
      break;
    };
    case common::SerializationType::Msgpack: {
      result = std::make_shared<common::MsgpackMessage>();
      serializeMsgpack(reply, *static_pointer_cast<common::MsgpackMessage>(result));
      break;
    };
    case common::SerializationType::MsgpackCompact: {
      result = std::make_shared<common::MsgpackMessage>();
      serializeMsgpackCompact(reply, *static_pointer_cast<common::MsgpackMessage>(result));
      break;
    };
  }
  return result;
}

/**
Pushable reply message
*/
class ReplyPushableMessage : public k2eg::service::pubsub::PublishMessage {
  const std::string                           queue;
  const std::string                           type;
  const std::string                           distribution_key;
  k2eg::common::ConstSerializedMessageShrdPtr message;
  k2eg::common::ConstDataUPtr                 message_data;

 public:
  ReplyPushableMessage(const std::string&                          queue,
                       const std::string&                          type,
                       const std::string&                          distribution_key,
                       k2eg::common::ConstSerializedMessageShrdPtr message);
  virtual ~ReplyPushableMessage();
  char*              getBufferPtr();
  const size_t       getBufferSize();
  const std::string& getQueue();
  const std::string& getDistributionKey();
  const std::string& getReqType();
};
DEFINE_PTR_TYPES(ReplyPushableMessage)
/**
 */
class WorkerAsyncOperation {
  const std::chrono::milliseconds       timeout_ms;
  const std::chrono::steady_clock::time_point begin;
 public:
  WorkerAsyncOperation(std::chrono::milliseconds timeout_ms) : timeout_ms(timeout_ms),  begin(std::chrono::steady_clock::now()) {}
  bool
  isTimeout() {
    return (std::chrono::steady_clock::now() - begin) > timeout_ms;
  }
};

/**
 */
class CommandWorker {
 public:
  CommandWorker()                                                                          = default;
  CommandWorker(const CommandWorker&)                                                      = delete;
  CommandWorker& operator=(const CommandWorker&)                                           = delete;
  ~CommandWorker()                                                                         = default;
  virtual void processCommand(std::shared_ptr<BS::light_thread_pool>  command_pool, k2eg::controller::command::cmd::ConstCommandShrdPtr command) = 0;
  virtual bool isReady();
  virtual std::size_t getTaskRunning() const;
};
DEFINE_PTR_TYPES(CommandWorker)
}  // namespace k2eg::controller::node::worker

#endif  // k2eg_CONTROLLER_NODE_WORKER_COMMANDWORKER_H_