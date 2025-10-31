#include "../metric/metric.h"
#include "common.h"
#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <k2eg/common/uuid.h>
#include <k2eg/service/ServiceResolver.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/log/impl/BoostLogger.h>
#include <k2eg/service/metric/IMetricService.h>
#include <k2eg/service/metric/impl/prometheus/PrometheusMetricService.h>
#include <k2eg/service/pubsub/IPublisher.h>
#include <k2eg/service/pubsub/pubsub.h>
#include <string>
#include <thread>
#include <unistd.h>

using namespace k2eg::common;
using namespace k2eg::service::pubsub;
using namespace k2eg::service::pubsub::impl::kafka;

// setup the test environment
using namespace k2eg::service;
using namespace k2eg::service::metric;
using namespace k2eg::service::metric::impl;
using namespace k2eg::service::metric::impl::prometheus_impl;
using namespace k2eg::service::log;
using namespace k2eg::service::log::impl;

class Kafka : public ::testing::Test
{
protected:
    Kafka() {}

    virtual ~Kafka() {}

    virtual void SetUp()
    {
        ServiceResolver<ILogger>::registerService<k2eg::service::log::ConstLogConfigurationShrdPtr, BoostLogger>(MakeLogConfigurationShrdPtr(LogConfiguration{}));
    }

    virtual void TearDown()
    {
        ServiceResolver<ILogger>::reset();
    }
};

#define TOPIC_TEST_NAME "queue-test"
#define LOG(x) std::cout << x << std::endl << std::flush

TEST_F(Kafka, KafkaFaultInitWithNoAddress)
{
    ASSERT_ANY_THROW(std::make_unique<RDKafkaPublisher>(std::make_unique<const PublisherConfiguration>(PublisherConfiguration{})););
    ASSERT_ANY_THROW(std::make_unique<RDKafkaSubscriber>(std::make_unique<const SubscriberConfiguration>(SubscriberConfiguration{})););
}

TEST_F(Kafka, KafkaAuthenticationTest)
{
    auto pub_conf = PublisherConfiguration{.server_address = "kafka:9092",
                                           .custom_impl_parameter = k2eg::common::MapStrKV{
                                               {"security.protocol", "SASL_PLAINTEXT"},
                                               {"sasl.mechanisms", "SCRAM-SHA-512"},
                                               {"sasl.username", "admin-user"},
                                               {"sasl.password", "admin-password"},
                                           }};
    auto sub_conf = SubscriberConfiguration{.server_address = "kafka:9092",
                                            .custom_impl_parameter = k2eg::common::MapStrKV{
                                                {"security.protocol", "SASL_PLAINTEXT"},
                                                {"sasl.mechanisms", "SCRAM-SHA-512"},
                                                {"sasl.username", "admin-user"},
                                                {"sasl.password", "admin-password"},
                                            }};
    ASSERT_NO_THROW(std::make_unique<RDKafkaPublisher>(std::make_unique<const PublisherConfiguration>(pub_conf)););
    ASSERT_NO_THROW(std::make_unique<RDKafkaSubscriber>(std::make_unique<const SubscriberConfiguration>(sub_conf)););
}

TEST_F(Kafka, CreateTopic)
{
    std::unique_ptr<RDKafkaPublisher> producer = std::make_unique<RDKafkaPublisher>(std::make_unique<const PublisherConfiguration>(PublisherConfiguration{.server_address = "kafka:9092"}));
    ASSERT_EQ(producer->createQueue(QueueDescription{.name = "new_queue", .paritions = 2, .replicas = 1, .retention_time = 1000 * 60 * 60, .retention_size = 1024 * 1024 * 1}), 0);
    SubscriberInterfaceElementVector data;
    std::unique_ptr<RDKafkaSubscriber> consumer = std::make_unique<RDKafkaSubscriber>(std::make_unique<const SubscriberConfiguration>(SubscriberConfiguration{.server_address = "kafka:9092"}));
    consumer->addQueue({"new_queue"});
    consumer->getMsg(data, 10);
    sleep(5);
    consumer->getMsg(data, 10);
    auto tipic_metadata = producer->getQueueMetadata("new_queue");
    consumer.reset();
    ASSERT_NE(tipic_metadata, nullptr);
    ASSERT_STREQ(tipic_metadata->name.c_str(), "new_queue");
    ASSERT_GE(tipic_metadata->subscriber_groups.size(), 1);
    ASSERT_EQ(producer->deleteQueue("new_queue"), 0);
}

TEST_F(Kafka, KafkaSimplePubSub)
{
    SubscriberInterfaceElementVector messages;
    std::unique_ptr<RDKafkaPublisher> producer = std::make_unique<RDKafkaPublisher>(std::make_unique<const PublisherConfiguration>(PublisherConfiguration{.server_address = "kafka:9092"}));
    std::unique_ptr<RDKafkaSubscriber> consumer = std::make_unique<RDKafkaSubscriber>(std::make_unique<const SubscriberConfiguration>(SubscriberConfiguration{.server_address = "kafka:9092"}));

    std::string message_sent = "hello_" + UUID::generateUUIDLite();
    ASSERT_NO_THROW(consumer->setQueue({TOPIC_TEST_NAME}));
    // give some times to consumer to register
    ASSERT_EQ(producer->createQueue(QueueDescription{.name = TOPIC_TEST_NAME, .paritions = 1, .retention_time = 1000 * 60 * 60, .retention_size = 1024 * 2}), 0);
    ASSERT_EQ(consumer->getMsg(messages, 1, 1000), 0);
    sleep(5);
    auto iotaFuture = std::async(
        std::launch::async,
        [&message_sent](std::unique_ptr<IPublisher> producer)
        {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            for (int idx = 0; idx <= 10; idx++)
            {
                ASSERT_EQ(producer->pushMessage(std::move(PublishMessageUniquePtr(new Message(TOPIC_TEST_NAME, message_sent))), {{"key-1", "value-1"}, {"key-2", "value-2"}}), 0);
            }
            ASSERT_EQ(producer->flush(1000), 0);
        },
        std::move(producer));
    iotaFuture.wait();

    ASSERT_EQ(consumer->getMsg(messages, 1, 1000), 0);
    while (messages.size() == 0)
    {
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

TEST_F(Kafka, KafkaSimplePubSubMultiple)
{
    SubscriberInterfaceElementVector messages;
    std::unique_ptr<RDKafkaPublisher> producer = std::make_unique<RDKafkaPublisher>(std::make_unique<const PublisherConfiguration>(PublisherConfiguration{.server_address = "kafka:9092"}));
    std::unique_ptr<RDKafkaSubscriber> consumer = std::make_unique<RDKafkaSubscriber>(std::make_unique<const SubscriberConfiguration>(SubscriberConfiguration{.server_address = "kafka:9092"}));

    std::string message_sent = "hello_" + UUID::generateUUIDLite();
    ASSERT_NO_THROW(consumer->setQueue({TOPIC_TEST_NAME}));
    // give some times to consumer to register
    ASSERT_EQ(producer->createQueue(QueueDescription{.name = TOPIC_TEST_NAME, .paritions = 1, .retention_time = 1000 * 60 * 60, .retention_size = 1024 * 2}), 0);
    ASSERT_EQ(consumer->getMsg(messages, 1, 1000), 0);
    sleep(5);
    auto iotaFuture = std::async(
        std::launch::async,
        [&message_sent](std::unique_ptr<IPublisher> producer)
        {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            for (int idx = 0; idx <= 10; idx++)
            {
                usleep(100000);
                ASSERT_EQ(producer->pushMessage(std::move(PublishMessageUniquePtr(new Message(TOPIC_TEST_NAME, message_sent))), {{"counter", std::to_string(idx)}, {"key-2", "value-2"}}), 0);
            }
            ASSERT_EQ(producer->flush(1000), 0);
        },
        std::move(producer));

    ASSERT_EQ(consumer->getMsg(messages, 10, 1000), 0);
    while (messages.size() < 10)
    {
        ASSERT_EQ(consumer->getMsg(messages, 10, 1000), 0);
    }
    iotaFuture.wait();
    ASSERT_EQ(messages.size(), 10);
    for (int idx = 0; idx < 10; idx++)
    {
        ASSERT_EQ(messages.at(idx)->header.size(), 2);
        ASSERT_NE(messages.at(idx)->header.find("counter"), std::end(messages.at(idx)->header));
        ASSERT_STREQ(messages.at(idx)->header.find("counter")->second.c_str(), std::to_string(idx).c_str());
    }
    ASSERT_NO_THROW(consumer->commit(););

    std::string message_received(messages[0]->data.get(), messages[0]->data_len);
    ASSERT_STREQ(message_received.c_str(), message_sent.c_str());
}

TEST_F(Kafka, KafkaSimplePubSubHeaderCheck)
{
    SubscriberInterfaceElementVector messages;
    std::unique_ptr<RDKafkaPublisher> producer = std::make_unique<RDKafkaPublisher>(std::make_unique<const PublisherConfiguration>(PublisherConfiguration{.server_address = "kafka:9092"}));
    std::unique_ptr<RDKafkaSubscriber> consumer = std::make_unique<RDKafkaSubscriber>(std::make_unique<const SubscriberConfiguration>(SubscriberConfiguration{.server_address = "kafka:9092"}));

    std::string message_sent = "hello_" + UUID::generateUUIDLite();
    ASSERT_NO_THROW(consumer->setQueue({TOPIC_TEST_NAME}));
    // give some times to consumer to register
    ASSERT_EQ(producer->createQueue(QueueDescription{.name = TOPIC_TEST_NAME, .paritions = 1, .retention_time = 1000 * 60 * 60, .retention_size = 1024 * 2}), 0);
    ASSERT_EQ(consumer->getMsg(messages, 1, 1000), 0);
    sleep(5);
    auto iotaFuture = std::async(
        std::launch::async,
        [&message_sent](std::unique_ptr<IPublisher> producer)
        {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            for (int idx = 0; idx <= 10; idx++)
            {
                ASSERT_EQ(producer->pushMessage(std::move(PublishMessageUniquePtr(new Message(TOPIC_TEST_NAME, message_sent)))), 0);
            }
            ASSERT_EQ(producer->flush(1000), 0);
        },
        std::move(producer));
    iotaFuture.wait();

    ASSERT_EQ(consumer->getMsg(messages, 1, 1000), 0);
    while (messages.size() == 0)
    {
        // LOG("Read message");
        ASSERT_EQ(consumer->getMsg(messages, 1, 1000), 0);
    }
    // LOG("Something has been read");
    ASSERT_NE(messages.size(), 0);
    ASSERT_NO_THROW(consumer->commit(););

    std::string message_received(messages[0]->data.get(), messages[0]->data_len);
    ASSERT_STREQ(message_received.c_str(), message_sent.c_str());
}

TEST_F(Kafka, KafkaPushMultipleMessage)
{
    SubscriberInterfaceElementVector tmp_received_messages;
    SubscriberInterfaceElementVector received_messages;
    std::unique_ptr<RDKafkaPublisher> producer = std::make_unique<RDKafkaPublisher>(std::make_unique<const PublisherConfiguration>(PublisherConfiguration{.server_address = "kafka:9092"}));
    std::unique_ptr<RDKafkaSubscriber> consumer = std::make_unique<RDKafkaSubscriber>(std::make_unique<const SubscriberConfiguration>(SubscriberConfiguration{.server_address = "kafka:9092"}));
    ASSERT_NO_THROW(consumer->setQueue({TOPIC_TEST_NAME}));
    // give some times to consumer to register
    sleep(5);
    PublisherMessageVector   push_messages;
    std::vector<std::string> message_to_sent;
    for (int idx = 0; idx < 10; idx++)
    {
        std::string message_sent = "hello_" + UUID::generateUUIDLite();
        message_to_sent.push_back(message_sent);
        push_messages.push_back(std::move(PublishMessageUniquePtr(new Message("queue-test", message_sent))));
    }

    auto iotaFuture = std::async(
        std::launch::async,
        [&push_messages](std::unique_ptr<IPublisher> producer)
        {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            ASSERT_EQ(producer->pushMessages(push_messages), 0);
            ASSERT_EQ(producer->flush(1000), 0);
        },
        std::move(producer));

    int to_fetch = message_to_sent.size();
    do
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        ASSERT_EQ(consumer->getMsg(tmp_received_messages, to_fetch, 1000), 0);
        received_messages.insert(received_messages.end(), tmp_received_messages.begin(), tmp_received_messages.end());
        tmp_received_messages.clear();
    } while (received_messages.size() != message_to_sent.size());
    ASSERT_NO_THROW(consumer->commit(););

    ASSERT_EQ(received_messages.size(), message_to_sent.size());

    for (int idx = 0; idx < received_messages.size(); idx++)
    {
        ASSERT_EQ(received_messages[idx]->data_len, message_to_sent[idx].size());
        ASSERT_EQ(std::memcmp(received_messages[idx]->data.get(), message_to_sent[idx].c_str(), received_messages[idx]->data_len), 0);
    }

    iotaFuture.wait();
}

TEST_F(Kafka, PublishingStressTest)
{
    SystemResourcePrinter            stat_printer;
    SubscriberInterfaceElementVector tmp_received_messages;
    SubscriberInterfaceElementVector received_messages;
    IMetricServiceUPtr               m_uptr;
    std::unique_ptr<RDKafkaPublisher> producer = std::make_unique<RDKafkaPublisher>(std::make_unique<const PublisherConfiguration>(PublisherConfiguration{.server_address = "kafka:9092"}));

    // give some times to consumer to register
    sleep(5);

    auto last_print = std::chrono::steady_clock::now();
    auto start_time = std::chrono::steady_clock::now();

    int idx = 0;
    stat_printer.printHeader();
    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time).count() < 60)
    {
        std::string             message_sent = "hello_" + UUID::generateUUIDLite();
        PublishMessageUniquePtr msg = std::move(PublishMessageUniquePtr(new Message("queue-test", message_sent)));
        ASSERT_EQ(producer->pushMessage(std::move(msg), {}), 0);

        auto now = std::chrono::steady_clock::now();
        if (idx == 0 || std::chrono::duration_cast<std::chrono::seconds>(now - last_print).count() >= 1)
        {
            stat_printer.refresh();
            stat_printer.printSample();
            last_print = now;
        }
        idx++;
    }
    producer.reset();
}
