#ifndef NODECONTROLLERCOMMON_H_
#define NODECONTROLLERCOMMON_H_

#include <latch>

#include "k2eg/common/ProgramOptions.h"
#include "k2eg/service/ServiceResolver.h"
#include "k2eg/service/log/ILogger.h"
#include "k2eg/service/log/impl/BoostLogger.h"
#include "k2eg/service/pubsub/IPublisher.h"

using namespace k2eg::common;
using namespace k2eg::service;
using namespace k2eg::service::log;
using namespace k2eg::service::log::impl;
using namespace k2eg::service::pubsub;
using namespace k2eg::controller::node::configuration;
using namespace k2eg::controller::node::worker::monitor;
using namespace k2eg::service::data;
using namespace k2eg::service::data::repository;

class ControllerConsumerDummyPublisher : public IPublisher {
  size_t consumer_number = 0;

 public:
  std::vector<PublishMessageSharedPtr> sent_messages;
  ControllerConsumerDummyPublisher() : IPublisher(std::make_unique<const PublisherConfiguration>(PublisherConfiguration{.server_address = "fake_address"})){};
  ~ControllerConsumerDummyPublisher() = default;
  void
  setAutoPoll(bool autopoll) {}
  int
  setCallBackForReqType(const std::string req_type, EventCallback eventCallback) {
    return 0;
  }
  int
  createQueue(const QueueDescription& queue) {
    return 0;
  }
  int
  deleteQueue(const std::string& queue_name) {
    return 0;
  }
  void
  setConsumerNumber(size_t consumer_number) {
    this->consumer_number = consumer_number;
  }

  QueueMetadataUPtr
  getQueueMetadata(const std::string& queue_name) {
    QueueMetadataUPtr qmt = std::make_unique<QueueMetadata>();
    for (int idx = 0; idx < consumer_number; idx++) {
      qmt->name = queue_name;

      std::vector<QueueSubscriberInfoUPtr> sub;
      sub.push_back(std::make_unique<QueueSubscriberInfo>(QueueSubscriberInfo{.client_id = "cid", .member_id = "mid", .host = "chost"}));

      qmt->subscriber_groups.push_back(std::make_unique<QueueSubscriberGroupInfo>(QueueSubscriberGroupInfo{
          .name        = "Group Name " + std::to_string(idx),
          .subscribers = std::move(sub),
      }));
    }
    return qmt;
  }
  int
  flush(const int timeo) {
    return 0;
  }
  int
  pushMessage(PublishMessageUniquePtr message, const PublisherHeaders& header = PublisherHeaders()) {
    PublishMessageSharedPtr message_shrd_ptr = std::move(message);
    sent_messages.push_back(message_shrd_ptr);
    return 0;
  }
  int
  pushMessages(PublisherMessageVector& messages, const PublisherHeaders& header = PublisherHeaders()) {
    for (auto& uptr : messages) { sent_messages.push_back(std::move(uptr)); }
    return 0;
  }
  size_t
  getQueueMessageSize() {
    return sent_messages.size();
  }
};



class DummyPublisher : public ControllerConsumerDummyPublisher {
  std::latch& lref;
 public:
  DummyPublisher(std::latch& lref)
      : ControllerConsumerDummyPublisher(), lref(lref){};
  ~DummyPublisher() = default;

  int
  pushMessage(PublishMessageUniquePtr message, const PublisherHeaders& header = PublisherHeaders()) {
    ControllerConsumerDummyPublisher::pushMessage(std::move(message), header);
    lref.count_down();
    return 0;
  }
  int
  pushMessages(PublisherMessageVector& messages, const PublisherHeaders& header = PublisherHeaders()) {
    ControllerConsumerDummyPublisher::pushMessages(messages, header);
    for (auto& uptr : messages) {
      lref.count_down();
    }
    return 0;
  }

};


#endif