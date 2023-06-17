#ifndef k2eg_CONTROLLER_NODE_WORKER_COMMANDWORKER_H_
#define k2eg_CONTROLLER_NODE_WORKER_COMMANDWORKER_H_
#include <k2eg/common/types.h>
#include <k2eg/service/pubsub/IPublisher.h>
#include <k2eg/controller/command/CMDCommand.h>

#include <chrono>
#include <k2eg/common/BS_thread_pool.hpp>
#include "k2eg/common/BaseSerialization.h"
namespace k2eg::controller::node::worker {

/**
Base command reply structure

Some command cand send a reply as result of operation,
thi class represent the base information for a reply
*/
struct CommandReply {
  //[mandatory] si the error code of the operation done by the command
  const std::int8_t error_code;
  //[mandatory] is the request id found on the command that has generated the reply
  const std::string reply_id;
};

static void
tag_invoke(boost::json::value_from_tag, boost::json::value &jv, CommandReply const &reply) {
  jv = {{"error_code", reply.error_code}, {KEY_REPLY_ID, reply.reply_id}};
}

inline common::SerializedMessageShrdPtr 
serializeJson(const CommandReply& reply){
  return common::SerializedMessageShrdPtr();
}
inline common::SerializedMessageShrdPtr 
serializeMsgpack(const CommandReply& reply){
  return common::SerializedMessageShrdPtr();
}
inline common::SerializedMessageShrdPtr 
serializeMsgpackCompact(const CommandReply& reply){
  return common::SerializedMessageShrdPtr();
}

inline common::SerializedMessageShrdPtr 
serialize(const CommandReply& reply, k2eg::common::SerializationType ser_type) {
  common::SerializedMessageShrdPtr result;
  switch (ser_type) {
    case common::SerializationType::Unknown: return common::SerializedMessageShrdPtr();
    case common::SerializationType::JSON: return serializeJson(reply);
    case common::SerializationType::Msgpack: return serializeMsgpack(reply);
    case common::SerializationType::MsgpackCompact: return serializeMsgpackCompact(reply);
  }
  return result;
}



/**
Pushable reply message
*/
class ReplyPushableMessage : public k2eg::service::pubsub::PublishMessage {
    const std::string queue;
    const std::string& type;
    const std::string request_type;
    const std::string distribution_key;
    k2eg::common::ConstSerializedMessageShrdPtr message;
    k2eg::common::ConstDataUPtr message_data;
public:
    ReplyPushableMessage(
        const std::string& queue, 
        const std::string& type,
        const std::string& distribution_key, 
        k2eg::common::ConstSerializedMessageShrdPtr  message);
    virtual ~ReplyPushableMessage() = default;
    char* getBufferPtr();
    const size_t getBufferSize();
    const std::string& getQueue();
    const std::string& getDistributionKey();
    const std::string& getReqType();
};
DEFINE_PTR_TYPES(ReplyPushableMessage)
/**
*/
class WorkerAsyncOperation {
  const std::chrono::milliseconds       timeout_ms;
  std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

 public:
  WorkerAsyncOperation(std::chrono::milliseconds timeout_ms) : timeout_ms(timeout_ms) {}
  bool
  isTimeout() {
    std::chrono::steady_clock::time_point now     = std::chrono::steady_clock::now();
    auto                                  elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - begin);
    return elapsed > timeout_ms;
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
  virtual void processCommand(k2eg::controller::command::cmd::ConstCommandShrdPtr command) = 0;
};
DEFINE_PTR_TYPES(CommandWorker)
}  // namespace k2eg::controller::node::worker

#endif  // k2eg_CONTROLLER_NODE_WORKER_COMMANDWORKER_H_