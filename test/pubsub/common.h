#include <k2eg/common/uuid.h>
#include <k2eg/service/pubsub/pubsub.h>

class Message : public k2eg::service::pubsub::PublishMessage {
    const std::string request_type;
    const std::string distribution_key;
    const std::string queue;
    //! the message data
    const std::string message;
  
   public:
    Message(const std::string& queue, const std::string& message)
        : request_type("test"), distribution_key(k2eg::common::UUID::generateUUIDLite()), queue(queue), message(message) {}
    virtual ~Message() {}
  
    char*
    getBufferPtr() {
      return const_cast<char*>(message.c_str());
    }
    const size_t
    getBufferSize() {
      return message.size();
    }
    const std::string&
    getQueue() {
      return queue;
    }
    const std::string&
    getDistributionKey() {
      return distribution_key;
    }
    const std::string&
    getReqType() {
      return request_type;
    }
  };