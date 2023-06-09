#include <gtest/gtest.h>
#include <k2eg/common/uuid.h>
#include <k2eg/service/pubsub/pubsub.h>
#include <unistd.h>

#include <chrono>
#include <future>
#include <iostream>
#include <thread>
#include "k2eg/service/pubsub/IPublisher.h"

using namespace k2eg::common;
using namespace k2eg::service::pubsub;
using namespace k2eg::service::pubsub::impl::kafka;

#define TOPIC_TEST_NAME "queue-test"

class Message : public PublishMessage {
  const std::string request_type;
  const std::string distribution_key;
  const std::string queue;
  //! the message data
  const std::string message;

 public:
  Message(const std::string& queue, const std::string& message)
      : request_type("test"), distribution_key(UUID::generateUUIDLite()), queue(queue), message(message) {}
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

#define LOG(x) std::cout << x << std::endl << std::flush

TEST(Kafka, KafkaFaultInitWithNoAddress) {
  ASSERT_ANY_THROW(std::make_unique<RDKafkaPublisher>(std::make_unique<const PublisherConfiguration>(PublisherConfiguration{})););
  ASSERT_ANY_THROW(std::make_unique<RDKafkaSubscriber>(std::make_unique<const SubscriberConfiguration>(SubscriberConfiguration{})););
}

TEST(Kafka, KafkaSimplePubSub) {
  SubscriberInterfaceElementVector  messages;
  std::unique_ptr<RDKafkaPublisher> producer =
      std::make_unique<RDKafkaPublisher>(std::make_unique<const PublisherConfiguration>(PublisherConfiguration{.server_address = "kafka:9092"}));
  std::unique_ptr<RDKafkaSubscriber> consumer =
      std::make_unique<RDKafkaSubscriber>(std::make_unique<const SubscriberConfiguration>(SubscriberConfiguration{.server_address = "kafka:9092"}));

  std::string message_sent = "hello_" + UUID::generateUUIDLite();
  ASSERT_NO_THROW(consumer->setQueue({TOPIC_TEST_NAME}));
  // give some times to consumer to register
  ASSERT_EQ(producer->createQueue(TOPIC_TEST_NAME), 0);
  ASSERT_EQ(consumer->getMsg(messages, 1, 1000), 0);
  sleep(5);
  auto iotaFuture = std::async(
      std::launch::async,
      [&message_sent](std::unique_ptr<IPublisher> producer) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        for (int idx = 0; idx <= 10; idx++) {
          ASSERT_EQ(producer->pushMessage(std::move(PublishMessageUniquePtr(new Message(TOPIC_TEST_NAME, message_sent))), {{"key-1", "value-1"},{"key-2", "value-2"}}), 0);
        }
        ASSERT_EQ(producer->flush(1000), 0);
      },
      std::move(producer));
  iotaFuture.wait();

  ASSERT_EQ(consumer->getMsg(messages, 1, 1000), 0);
  while (messages.size() == 0) {
    ASSERT_EQ(consumer->getMsg(messages, 1, 1000), 0);
  }
  ASSERT_NE(messages.size(), 0);
  ASSERT_EQ(messages.at(0)->header.size(), 2);
  ASSERT_NE(messages.at(0)->header.find("key-1"), std::end(messages.at(0)->header));
  ASSERT_STREQ(messages.at(0)->header.find("key-1")->second.c_str(), "value-1");
  ASSERT_NO_THROW(consumer->commit(););

  std::string message_received(messages[0]->data.get(), messages[0]->data_len);
  ASSERT_STREQ(message_received.c_str(), message_sent.c_str());
}

TEST(Kafka, KafkaSimplePubSubHeaderCheck) {
  SubscriberInterfaceElementVector  messages;
  std::unique_ptr<RDKafkaPublisher> producer =
      std::make_unique<RDKafkaPublisher>(std::make_unique<const PublisherConfiguration>(PublisherConfiguration{.server_address = "kafka:9092"}));
  std::unique_ptr<RDKafkaSubscriber> consumer =
      std::make_unique<RDKafkaSubscriber>(std::make_unique<const SubscriberConfiguration>(SubscriberConfiguration{.server_address = "kafka:9092"}));

  std::string message_sent = "hello_" + UUID::generateUUIDLite();
  ASSERT_NO_THROW(consumer->setQueue({TOPIC_TEST_NAME}));
  // give some times to consumer to register
  ASSERT_EQ(producer->createQueue(TOPIC_TEST_NAME), 0);
  ASSERT_EQ(consumer->getMsg(messages, 1, 1000), 0);
  sleep(5);
  auto iotaFuture = std::async(
      std::launch::async,
      [&message_sent](std::unique_ptr<IPublisher> producer) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        for (int idx = 0; idx <= 10; idx++) {
          ASSERT_EQ(producer->pushMessage(std::move(PublishMessageUniquePtr(new Message(TOPIC_TEST_NAME, message_sent)))), 0);
        }
        ASSERT_EQ(producer->flush(1000), 0);
      },
      std::move(producer));
  iotaFuture.wait();

  ASSERT_EQ(consumer->getMsg(messages, 1, 1000), 0);
  while (messages.size() == 0) {
    // LOG("Read message");
    ASSERT_EQ(consumer->getMsg(messages, 1, 1000), 0);
  }
  // LOG("Something has been read");
  ASSERT_NE(messages.size(), 0);
  ASSERT_NO_THROW(consumer->commit(););

  std::string message_received(messages[0]->data.get(), messages[0]->data_len);
  ASSERT_STREQ(message_received.c_str(), message_sent.c_str());
}

TEST(Kafka, KafkaPushMultipleMessage) {
  SubscriberInterfaceElementVector  tmp_received_messages;
  SubscriberInterfaceElementVector  received_messages;
  std::unique_ptr<RDKafkaPublisher> producer =
      std::make_unique<RDKafkaPublisher>(std::make_unique<const PublisherConfiguration>(PublisherConfiguration{.server_address = "kafka:9092"}));
  std::unique_ptr<RDKafkaSubscriber> consumer =
      std::make_unique<RDKafkaSubscriber>(std::make_unique<const SubscriberConfiguration>(SubscriberConfiguration{.server_address = "kafka:9092"}));
  ASSERT_NO_THROW(consumer->setQueue({TOPIC_TEST_NAME}));
  // give some times to consumer to register
  sleep(5);
  PublisherMessageVector   push_messages;
  std::vector<std::string> message_to_sent;
  for (int idx = 0; idx < 10; idx++) {
    std::string message_sent = "hello_" + UUID::generateUUIDLite();
    message_to_sent.push_back(message_sent);
    push_messages.push_back(std::move(PublishMessageUniquePtr(new Message("queue-test", message_sent))));
  }

  auto iotaFuture = std::async(
      std::launch::async,
      [&push_messages](std::unique_ptr<IPublisher> producer) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        ASSERT_EQ(producer->pushMessages(push_messages), 0);
        ASSERT_EQ(producer->flush(1000), 0);
      },
      std::move(producer));

  int to_fetch = message_to_sent.size();
  do {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    ASSERT_EQ(consumer->getMsg(tmp_received_messages, to_fetch, 1000), 0);
    received_messages.insert(received_messages.end(), tmp_received_messages.begin(), tmp_received_messages.end());
    tmp_received_messages.clear();
  } while (received_messages.size() != message_to_sent.size());
  ASSERT_NO_THROW(consumer->commit(););

  ASSERT_EQ(received_messages.size(), message_to_sent.size());

  for (int idx = 0; idx < received_messages.size(); idx++) {
    ASSERT_EQ(received_messages[idx]->data_len, message_to_sent[idx].size());
    ASSERT_EQ(std::memcmp(received_messages[idx]->data.get(), message_to_sent[idx].c_str(), received_messages[idx]->data_len), 0);
  }

  iotaFuture.wait();
}
