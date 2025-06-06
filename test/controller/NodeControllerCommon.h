#ifndef NODECONTROLLERCOMMON_H_
#define NODECONTROLLERCOMMON_H_

#include <gtest/gtest.h>

#include <latch>
#include <map>
#include <string>

#include "boost/json/object.hpp"
#include "k2eg/common/ProgramOptions.h"
#include "k2eg/service/ServiceResolver.h"
#include "k2eg/service/log/ILogger.h"
#include "k2eg/service/log/impl/BoostLogger.h"
#include "k2eg/service/pubsub/IPublisher.h"
#include <k2eg/common/utility.h>
#include <k2eg/controller/node/NodeController.h>
#include <k2eg/service/ServiceResolver.h>
#include <k2eg/service/configuration/configuration.h>
#include <k2eg/service/data/DataStorage.h>
#include <k2eg/service/epics/EpicsServiceManager.h>
#include <k2eg/service/metric/IMetricService.h>
#include <k2eg/service/metric/impl/prometheus/PrometheusMetricService.h>
#include <k2eg/service/pubsub/pubsub.h>
#include <k2eg/service/scheduler/Scheduler.h>

#include <filesystem>

using namespace k2eg::common;

using namespace k2eg::controller::node::configuration;
using namespace k2eg::controller::node::worker::monitor;
using namespace k2eg::controller::command;
using namespace k2eg::controller::command::cmd;
using namespace k2eg::controller::node;

using namespace k2eg::service;
using namespace k2eg::service::log;
using namespace k2eg::service::log::impl;
using namespace k2eg::service::pubsub;
using namespace k2eg::service;
using namespace k2eg::service::data;
using namespace k2eg::service::data::repository;
using namespace k2eg::service::log;
using namespace k2eg::service::log::impl;
using namespace k2eg::service::pubsub;
using namespace k2eg::service::data;
using namespace k2eg::service::scheduler;
using namespace k2eg::service::epics_impl;
using namespace k2eg::service::configuration;
using namespace k2eg::service::configuration::impl::consul;
using namespace k2eg::service::metric;
using namespace k2eg::service::metric::impl::prometheus_impl;
namespace bs = boost::system;
namespace bj = boost::json;
namespace fs = std::filesystem;

class DummyPublisherCounter : public IPublisher
{
    std::uint64_t counter;

public:
    std::latch l;
    DummyPublisherCounter(unsigned int latch_counter)
        : IPublisher(std::make_unique<const PublisherConfiguration>(PublisherConfiguration{.server_address = "fake_" "a" "d" "d" "r" "e" "s" "s"})), l(latch_counter), counter(0) {};
    ~DummyPublisherCounter() = default;

    void setAutoPoll(bool autopoll) {}

    int setCallBackForReqType(const std::string req_type, EventCallback eventCallback)
    {
        return 0;
    }

    int createQueue(const QueueDescription& queue)
    {
        return 0;
    }

    int deleteQueue(const std::string& queue_name)
    {
        return 0;
    }

    QueueMetadataUPtr getQueueMetadata(const std::string& queue_name)
    {
        return nullptr;
    }

    int flush(const int timeo)
    {
        return 0;
    }

    int pushMessage(PublishMessageUniquePtr message, const PublisherHeaders& header = PublisherHeaders())
    {
        counter++;
        l.count_down();
        return 0;
    }

    int pushMessages(PublisherMessageVector& messages, const PublisherHeaders& header = PublisherHeaders())
    {
        counter += messages.size();
        l.count_down(messages.size());
        return 0;
    }

    size_t getQueueMessageSize()
    {
        return counter;
    }
};

class ControllerConsumerDummyPublisher : public IPublisher
{
    size_t consumer_number = 0;

public:
    std::vector<PublishMessageSharedPtr> sent_messages;
    ControllerConsumerDummyPublisher()
        : IPublisher(std::make_unique<const PublisherConfiguration>(PublisherConfiguration{.server_address = "fake_" "a" "d" "d" "r" "e" "s" "s"})) {};
    ~ControllerConsumerDummyPublisher() = default;

    void setAutoPoll(bool autopoll) {}

    int setCallBackForReqType(const std::string req_type, EventCallback eventCallback)
    {
        return 0;
    }

    int createQueue(const QueueDescription& queue)
    {
        return 0;
    }

    int deleteQueue(const std::string& queue_name)
    {
        return 0;
    }

    void setConsumerNumber(size_t consumer_number)
    {
        this->consumer_number = consumer_number;
    }

    QueueMetadataUPtr getQueueMetadata(const std::string& queue_name)
    {
        QueueMetadataUPtr qmt = std::make_unique<QueueMetadata>();
        for (int idx = 0; idx < consumer_number; idx++)
        {
            qmt->name = queue_name;

            std::vector<QueueSubscriberInfoUPtr> sub;
            sub.push_back(std::make_unique<QueueSubscriberInfo>(QueueSubscriberInfo{.client_id = "cid", .member_id = "mid", .host = "chost"}));

            qmt->subscriber_groups.push_back(std::make_unique<QueueSubscriberGroupInfo>(QueueSubscriberGroupInfo{
                .name = "Group Name " + std::to_string(idx),
                .subscribers = std::move(sub),
            }));
        }
        return qmt;
    }

    int flush(const int timeo)
    {
        return 0;
    }

    int pushMessage(PublishMessageUniquePtr message, const PublisherHeaders& header = PublisherHeaders())
    {
        PublishMessageSharedPtr message_shrd_ptr = std::move(message);
        sent_messages.push_back(message_shrd_ptr);
        return 0;
    }

    int pushMessages(PublisherMessageVector& messages, const PublisherHeaders& header = PublisherHeaders())
    {
        for (auto& uptr : messages)
        {
            sent_messages.push_back(std::move(uptr));
        }
        return 0;
    }

    size_t getQueueMessageSize()
    {
        return sent_messages.size();
    }
};

class DummyPublisher : public ControllerConsumerDummyPublisher
{
    std::latch& lref;

public:
    DummyPublisher(std::latch& lref) : ControllerConsumerDummyPublisher(), lref(lref) {};
    ~DummyPublisher() = default;

    int pushMessage(PublishMessageUniquePtr message, const PublisherHeaders& header = PublisherHeaders())
    {
        ControllerConsumerDummyPublisher::pushMessage(std::move(message), header);
        lref.count_down();
        return 0;
    }

    int pushMessages(PublisherMessageVector& messages, const PublisherHeaders& header = PublisherHeaders())
    {
        ControllerConsumerDummyPublisher::pushMessages(messages, header);
        for (auto& uptr : messages)
        {
            lref.count_down();
        }
        return 0;
    }
};

/**
wait for message that came from all the registered topics
*/
class TopicTargetPublisher : public ControllerConsumerDummyPublisher
{
    std::latch               lref;
    std::vector<std::string> topics;

public:
    bool enable_log = false;
    TopicTargetPublisher(std::vector<std::string>& tvec)
        : ControllerConsumerDummyPublisher(), lref(tvec.size()), topics(tvec) {};
    ~TopicTargetPublisher() = default;

    std::latch& getLatch()
    {
        return lref;
    }

    int pushMessage(PublishMessageUniquePtr message, const PublisherHeaders& header = PublisherHeaders())
    {
        // check if array contains topic
        auto it = std::find_if(topics.begin(), topics.end(),
                               [&message, this](const std::string& topic)
                               {
                                   if (this->enable_log)
                                   {
                                       std::cout << "check: " << topic << "against: " << message->getQueue() << "<==" << std::endl;
                                   }
                                   return message->getQueue().compare(topic) == 0;
                               });
        if (it != topics.end())
        {
            if (enable_log)
            {
                std::cout << "Topic found: " << *it << std::endl;
            }
            lref.count_down();
            ControllerConsumerDummyPublisher::pushMessage(std::move(message), header);
            // remove this topic
            topics.erase(it);
        }
        else
        {
            ControllerConsumerDummyPublisher::pushMessage(std::move(message), header);
        }
        return 0;
    }

    int pushMessages(PublisherMessageVector& messages, const PublisherHeaders& header = PublisherHeaders())
    {
        for (auto& uptr : messages)
        {
            auto it = std::find_if(topics.begin(), topics.end(),
                                   [&uptr, this](const std::string& topic)
                                   {
                                       if (this->enable_log)
                                       {
                                           std::cout << "check: " << topic << "against: " << uptr->getQueue() << "<==" << std::endl;
                                       }
                                       return uptr->getQueue().compare(topic) == 0;
                                   });
            if (it != topics.end())
            {
                if (enable_log)
                {
                    std::cout << "Topic found: " << *it << std::endl;
                }
                lref.count_down();
                ControllerConsumerDummyPublisher::pushMessage(std::move(uptr), header);
                // remove this topic
                topics.erase(it);
            }
            else
            {
                ControllerConsumerDummyPublisher::pushMessage(std::move(uptr), header);
            }
        }
        return 0;
    }
};

/**
 * @class TopicCountedTargetPublisher
 * @brief A test publisher that tracks received Kafka messages by topic and allows waiting
 *        until a specified number of messages have been received per topic.
 *
 * This class is designed for use in integration tests where you want to verify that
 * specific topics have received a certain number of messages. It extends the
 * ControllerConsumerDummyPublisher and overrides `pushMessage` and `pushMessages` to
 * track message counts per topic. It supports reuse across multiple test phases by
 * allowing reinitialization of topic expectations using `setExpectedTopics()`.
 *
 * Features:
 * - Thread-safe tracking of received messages by topic.
 * - Wait mechanism using std::condition_variable to block until all topics are fulfilled.
 * - Supports both individual and batched message handling.
 * - Reusable for multiple wait cycles in the same test session.
 */
class TopicCountedTargetPublisher : public ControllerConsumerDummyPublisher
{
    std::mutex                 mtx;
    std::condition_variable    cv;
    std::map<std::string, int> topic_counts;
    std::map<std::string, int> received_counts; // Track received per topic
    int                        remaining_topics = 0;

public:
    bool enable_log = false;

    TopicCountedTargetPublisher() = default;

    // Wait until all expected topics are fulfilled, then return received_counts
    std::map<std::string, int> wait(const std::map<std::string, int>& expected)
    {
        {
            std::unique_lock lock(mtx);
            topic_counts = expected;
            received_counts.clear();
            remaining_topics = static_cast<int>(topic_counts.size());
        }
        std::unique_lock lock(mtx);
        cv.wait(lock,
                [this]
                {
                    return remaining_topics == 0;
                });
        return received_counts;
    }

    // Wait with timeout, return received_counts (may be incomplete if timeout)
    std::map<std::string, int> wait_for(const std::map<std::string, int>& expected, std::chrono::milliseconds duration)
    {
        {
            std::unique_lock lock(mtx);
            topic_counts = expected;
            received_counts.clear();
            remaining_topics = static_cast<int>(topic_counts.size());
        }
        std::unique_lock lock(mtx);
        cv.wait_for(lock, duration,
                    [this]
                    {
                        return remaining_topics == 0;
                    });
        return received_counts;
    }

    int pushMessage(PublishMessageUniquePtr message, const PublisherHeaders& header = PublisherHeaders()) override
    {
        std::string queue = message->getQueue();

        {
            std::unique_lock lock(mtx);
            auto             it = topic_counts.find(queue);
            if (it != topic_counts.end())
            {
                received_counts[queue]++;
                if (--it->second == 0)
                {
                    topic_counts.erase(it);
                    --remaining_topics;
                    if (enable_log)
                    {
                        std::cout << "[pushMessage] Topic complete: " << queue << "\n";
                    }
                    if (remaining_topics == 0)
                    {
                        cv.notify_all();
                    }
                }
            }
        }

        return ControllerConsumerDummyPublisher::pushMessage(std::move(message), header);
    }

    int pushMessages(PublisherMessageVector& messages, const PublisherHeaders& header = PublisherHeaders()) override
    {
        {
            std::unique_lock lock(mtx);
            for (auto& uptr : messages)
            {
                std::string queue = uptr->getQueue();
                auto        it = topic_counts.find(queue);
                if (it != topic_counts.end())
                {
                    received_counts[queue]++;
                    if (--it->second == 0)
                    {
                        topic_counts.erase(it);
                        --remaining_topics;
                        if (enable_log)
                        {
                            std::cout << "[pushMessages] Topic complete: " << queue << "\n";
                        }
                        if (remaining_topics == 0)
                        {
                            cv.notify_all();
                        }
                    }
                }
                ControllerConsumerDummyPublisher::pushMessage(std::move(uptr), header);
            }
        }

        return 0;
    }
};

class DummyPublisherNoSignal : public IPublisher
{
public:
    std::vector<PublishMessageSharedPtr> sent_messages;
    DummyPublisherNoSignal()
        : IPublisher(std::make_unique<const PublisherConfiguration>(PublisherConfiguration{.server_address = "fake_" "a" "d" "d" "r" "e" "s" "s"})) {};
    ~DummyPublisherNoSignal() = default;

    void setAutoPoll(bool autopoll) {}

    int setCallBackForReqType(const std::string req_type, EventCallback eventCallback)
    {
        return 0;
    }

    int createQueue(const QueueDescription& queue)
    {
        return 0;
    }

    int deleteQueue(const std::string& queue_name)
    {
        return 0;
    }

    QueueMetadataUPtr getQueueMetadata(const std::string& queue_name)
    {
        return nullptr;
    }

    int flush(const int timeo)
    {
        return 0;
    }

    int pushMessage(PublishMessageUniquePtr message, const PublisherHeaders& header = PublisherHeaders())
    {
        sent_messages.push_back(std::move(message));
        return 0;
    }

    int pushMessages(PublisherMessageVector& messages, const PublisherHeaders& header = PublisherHeaders())
    {
        messages.clear();
        return 0;
    }

    size_t getQueueMessageSize()
    {
        return sent_messages.size();
    }
};

class DischargePublisher : public ControllerConsumerDummyPublisher
{
public:
    DischargePublisher() = default;
    ~DischargePublisher() = default;

    void setAutoPoll(bool autopoll) {}

    int setCallBackForReqType(const std::string req_type, EventCallback eventCallback)
    {
        return 0;
    }

    int createQueue(const QueueDescription& queue)
    {
        return 0;
    }

    int deleteQueue(const std::string& queue_name)
    {
        return 0;
    }
    
    int pushMessage(PublishMessageUniquePtr message, const PublisherHeaders& header = PublisherHeaders())
    {
        return 0;
    }

    int pushMessages(PublisherMessageVector& messages, const PublisherHeaders& header = PublisherHeaders())
    {
        return 0;
    }

    size_t getQueueMessageSize()
    {
        return 0;
    }
};

inline boost::json::object getJsonObject(PublishMessage& published_message)
{
    bs::error_code  ec;
    bj::string_view value_str = bj::string_view(published_message.getBufferPtr(), published_message.getBufferSize());
    auto            result = bj::parse(value_str, ec).as_object();
    if (ec)
        throw std::runtime_error("invalid json");
    return result;
}

inline msgpack::unpacked getMsgPackObject(PublishMessage& published_message)
{
    msgpack::unpacked msg_upacked;
    msgpack::unpack(msg_upacked, published_message.getBufferPtr(), published_message.getBufferSize());
    return msg_upacked;
}

inline void wait_forPublished_message_size(DummyPublisherNoSignal& publisher, unsigned int requested_size, unsigned int timeout_ms)
{
    auto start_time = std::chrono::steady_clock::now();
    auto end_time = std::chrono::steady_clock::now();
    std::chrono::duration<long, std::milli> tout = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    bool waiting = true;
    while (waiting)
    {
        waiting = publisher.getQueueMessageSize() < requested_size;
        waiting = waiting && (tout.count() < timeout_ms);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        end_time = std::chrono::steady_clock::now();
        tout = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    }
}

inline boost::json::object exstractJsonObjectThatContainsKey(std::vector<PublishMessageSharedPtr>& messages, const std::string& key_to_find, const std::string& published_on_topic, bool log = false)
{
    for (int idx = 0; idx < messages.size(); idx++)
    {
        std::cout << "queue:" << messages[idx]->getQueue() << std::endl;
        if (messages[idx]->getQueue().compare(published_on_topic) != 0)
            continue;
        auto json_obj = getJsonObject(*messages[idx]);
        if (log)
        {
            std::cout << json_obj << std::endl;
        }
        if (json_obj.contains(key_to_find))
        {
            return json_obj;
        }
    }
    return boost::json::object();
}

inline msgpack::unpacked exstractMsgpackObjectThatContainsKey(std::vector<PublishMessageSharedPtr>& messages, const std::string& key_to_find, const std::string& published_on_topic, bool log = false)
{
    typedef std::map<std::string, msgpack::object> Map;
    typedef std::vector<msgpack::object>           Vec;
    for (int idx = 0; idx < messages.size(); idx++)
    {
        if (messages[idx]->getQueue().compare(published_on_topic) != 0)
            continue;
        auto msgpack_obj = getMsgPackObject(*messages[idx]);
        switch (msgpack_obj->type)
        {
        case msgpack::type::MAP:
            {
                auto map_reply = msgpack_obj->as<Map>();
                if (map_reply.contains(key_to_find))
                {
                    return msgpack_obj;
                }
                break;
            }

        case msgpack::type::ARRAY:
            {
                return msgpack_obj;
            }
        }
    }
    return msgpack::unpacked();
}

inline msgpack::unpacked exstractMsgpackObjectAtIndex(
    std::vector<PublishMessageSharedPtr>& messages, 
    const std::string& published_on_topic, 
    const int message_idx,
    bool log = false)
{
    int curerent_idx = 0;
    typedef std::map<std::string, msgpack::object> Map;
    typedef std::vector<msgpack::object>           Vec;
    if(message_idx < 0 || message_idx >= messages.size())
    {
        throw std::out_of_range("message index out of range");
    }
     msgpack::unpacked result = msgpack::unpacked();
    for (int idx = 0; idx < messages.size(); idx++)
    {
        if(messages[idx] == nullptr)
        {
            continue;
        }
        if (messages[idx]->getQueue().compare(published_on_topic) != 0)
            continue;
        if (curerent_idx++ != message_idx)
            continue;
        result = getMsgPackObject(*messages[idx]);
        if (log)
        {
            std::cout << result.get() << std::endl;
        }
    }
    return result;
}

inline msgpack::object getMSGPackObjectForKey(const msgpack::object& o, const std::string& key){
    if (o.type != msgpack::type::MAP) return msgpack::object();

    auto map_reply = o.as<std::map<std::string, msgpack::object>>();
    auto it = map_reply.find(key);
    if (it != map_reply.end())
    {
        return it->second;
    } else {
        return msgpack::object();
    }
}

inline bool checkMSGPackObjectContains(const msgpack::object& o, const std::string& key){
    if (o.type != msgpack::type::MAP) return false;

    auto map_reply = o.as<std::map<std::string, msgpack::object>>();
    auto it = map_reply.find(key);
    return it != map_reply.end();
}

inline std::size_t countMessageOnTopic(std::vector<PublishMessageSharedPtr>& messages, const std::string& published_on_topic)
{
    std::size_t counter = 0;
    for (int idx = 0; idx < messages.size(); idx++)
    {
        if (messages[idx]->getQueue().compare(published_on_topic) != 0)
            continue;
        counter++;
    }
    return counter;
}

inline std::unique_ptr<NodeController> initBackend(int& tcp_port, IPublisherShrdPtr pub, bool enable_debug_log = false, bool reset_conf = true)
{
    int         argc = 1;
    const char* argv[1] = {"epics-k2eg-test"};
    clearenv();
    if (enable_debug_log)
    {
        setenv("EPICS_k2eg_log-on-console", "true", 1);
        setenv("EPICS_k2eg_log-level", "trace", 1);
    }
    else
    {
        setenv("EPICS_k2eg_log-on-console", "false", 1);
    }

    if (reset_conf)
    {
        setenv("EPICS_k2eg_configuration-reset-on-start", "true", 1);
    }

    setenv("EPICS_k2eg_metric-server-http-port", std::to_string(++tcp_port).c_str(), 1);
    setenv(("EPICS_k2eg_" + std::string(SCHEDULER_CHECK_EVERY_AMOUNT_OF_SECONDS)).c_str(), "1", 1);
    // set monitor expiration time out at minimum
    setenv(("EPICS_k2eg_" + std::string(NC_MONITOR_EXPIRATION_TIMEOUT)).c_str(), "1", 1);
    setenv(("EPICS_k2eg_" + std::string(CONFIGURATION_SERVICE_HOST)).c_str(), "consul", 1);
    setenv(("EPICS_k2eg_" + std::string(METRIC_ENABLE)).c_str(), "true", 1);
    setenv(("EPICS_k2eg_" + std::string(METRIC_HTTP_PORT)).c_str(), "8080", 1);
    std::unique_ptr<ProgramOptions> opt = std::make_unique<ProgramOptions>();
    opt->parse(argc, argv);
    ServiceResolver<INodeConfiguration>::registerService(std::make_shared<ConsuleNodeConfiguration>(opt->getConfigurationServiceConfiguration()));
    ServiceResolver<Scheduler>::registerService(std::make_shared<Scheduler>(opt->getSchedulerConfiguration()));
    ServiceResolver<Scheduler>::resolve()->start();
    ServiceResolver<ILogger>::registerService(std::make_shared<BoostLogger>(opt->getloggerConfiguration()));
    ServiceResolver<IMetricService>::registerService(std::make_shared<PrometheusMetricService>(opt->getMetricConfiguration()));
    ServiceResolver<EpicsServiceManager>::registerService(std::make_shared<EpicsServiceManager>());
    ServiceResolver<IPublisher>::registerService(pub);
    DataStorageUPtr storage = std::make_unique<DataStorage>(fs::path(fs::current_path()) / "test.sqlite");
    // data should be alwasys erased becasue now the local databas eis only used at runtime the persistent
    // data is stored on the central configuration management server
    toShared(storage->getChannelRepository())->removeAll();
    return std::make_unique<NodeController>(opt->getNodeControllerConfiguration(), std::move(storage));
}

inline void deinitBackend(std::unique_ptr<NodeController> node_controller)
{
    node_controller.reset();
    EXPECT_NO_THROW(ServiceResolver<IPublisher>::resolve().reset(););
    EXPECT_NO_THROW(ServiceResolver<EpicsServiceManager>::resolve().reset(););
    EXPECT_NO_THROW(ServiceResolver<IMetricService>::resolve().reset(););
    EXPECT_NO_THROW(ServiceResolver<ILogger>::resolve().reset(););
    EXPECT_NO_THROW(ServiceResolver<Scheduler>::resolve()->stop(););
    EXPECT_NO_THROW(ServiceResolver<Scheduler>::resolve().reset(););
    EXPECT_NO_THROW(ServiceResolver<INodeConfiguration>::resolve().reset(););
}
#endif