#ifndef RDKKAFKAPUBLISHER_H
#define RDKKAFKAPUBLISHER_H

#pragma once
#include <k2eg/service/pubsub/IPublisher.h>
#include <k2eg/service/pubsub/impl/kafka/RDKafkaBase.h>
#include <librdkafka/rdkafka.h>
#include <librdkafka/rdkafkacpp.h>

#include <memory>
#include <thread>

// smart pointer delete for rd_kafka_queue_t
namespace k2eg::service::pubsub::impl::kafka {
struct RdKafkaQueueDeleter {
  void
  operator()(rd_kafka_queue_t* queue) {
    rd_kafka_queue_destroy(queue);
  }
};

// smart pointer delete for rd_kafka_AdminOptions_t
struct RdKafkaAdminOptionDeleter {
  void
  operator()(rd_kafka_AdminOptions_t* option) {
    rd_kafka_AdminOptions_destroy(option);
  }
};

// smart pointer delete for rd_kafka_DeleteTopic_t
struct RdKafkaDeleteTopicArrayDeleter {
  const size_t count;
  RdKafkaDeleteTopicArrayDeleter(size_t count) : count(count) {}
  void
  operator()(rd_kafka_DeleteTopic_t** delete_topic_array) {
    rd_kafka_DeleteTopic_destroy_array(delete_topic_array, count);
  }
};

// smart pointer delete for rd_kafka_NewTopic_t**
struct RdKafkaNewTopicArrayDeleter {
  const size_t count;
  RdKafkaNewTopicArrayDeleter(size_t count) : count(count) {}
  void
  operator()(rd_kafka_NewTopic_t** delete_topic_array) {
    rd_kafka_NewTopic_destroy_array(delete_topic_array, count);
  }
};

// smart pointer deallocator for event
struct RdKafkaEventDeleter {
  void
  operator()(rd_kafka_event_t* event) {
    rd_kafka_event_destroy(event);
  }
};

// published implementation usin librdkafka
class RDKafkaPublisher : public IPublisher, RDKafkaBase, RdKafka::DeliveryReportCb {
  bool                               _stop_inner_thread;
  bool                               _auto_poll;
  std::thread                        auto_poll_thread;
  std::unique_ptr<RdKafka::Producer> producer;

  rd_kafka_event_t* wait_admin_result(rd_kafka_queue_t* q, rd_kafka_event_type_t evtype, int tmout);

 protected:
  void         dr_cb(RdKafka::Message& message);
  void         autoPoll();
  virtual void init();
  virtual void deinit();

 public:
  explicit RDKafkaPublisher(ConstPublisherConfigurationUPtr configuration);
  virtual ~RDKafkaPublisher();
  virtual int    createQueue(const QueueDescription& new_queue);
  virtual int    deleteQueue(const std::string& queue_name);
  virtual void   setAutoPoll(bool autopoll);
  virtual int    flush(const int timeo = 10000);
  virtual int    pushMessage(PublishMessageUniquePtr message, const PublisherHeaders& headers = PublisherHeaders());
  virtual int    pushMessages(PublisherMessageVector& messages, const PublisherHeaders& headers = PublisherHeaders());
  virtual size_t getQueueMessageSize();
};
}  // namespace k2eg::service::pubsub::impl::kafka

#endif