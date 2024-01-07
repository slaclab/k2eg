#include <k2eg/controller/node/worker/CommandWorker.h>

using namespace k2eg::common;
using namespace k2eg::controller::node::worker;

ReplyPushableMessage::ReplyPushableMessage(const std::string&          queue,
                                           const std::string&          type,
                                           const std::string&          distribution_key,
                                           k2eg::common::ConstSerializedMessageShrdPtr  message)
    : type(type), queue(queue), distribution_key(distribution_key),message(message), message_data(this->message->data()) {}
char*
ReplyPushableMessage::getBufferPtr() {
  return const_cast<char*>(message_data->data());
}
const size_t
ReplyPushableMessage::getBufferSize() {
  return message_data->size();
}
const std::string&
ReplyPushableMessage::getQueue() {
  return queue;
}
const std::string&
ReplyPushableMessage::getDistributionKey() {
  return distribution_key;
}
const std::string&
ReplyPushableMessage::getReqType() {
  return type;
}

bool 
CommandWorker::isReady() {
  return true;
}