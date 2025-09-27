#include <k2eg/common/uuid.h>

#include <k2eg/service/pubsub/ISubscriber.h>
#include <k2eg/service/pubsub/impl/kafka/RDKafkaSubscriber.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

using namespace k2eg::common;
using namespace k2eg::service::pubsub::impl::kafka;

RDKafkaSubscriber::RDKafkaSubscriber(ConstSubscriberConfigurationShrdPtr configuration)
    : ISubscriber(std::move(configuration)), RDKafkaBase()
{
    k2eg::common::MapStrKV merged = this->configuration ? this->configuration->custom_impl_parameter : k2eg::common::MapStrKV{};
    for (const auto& kv : this->getRuntimeOverrides())
    {
        merged[kv.first] = kv.second;
    }
    init(merged);
}

RDKafkaSubscriber::RDKafkaSubscriber(ConstSubscriberConfigurationShrdPtr              configuration,
                                     const std::unordered_map<std::string, std::any>& overrides)
    : ISubscriber(std::move(configuration), overrides), RDKafkaBase()
{
    k2eg::common::MapStrKV merged = this->configuration ? this->configuration->custom_impl_parameter : k2eg::common::MapStrKV{};
    for (const auto& kv : this->getRuntimeOverrides())
    {
        merged[kv.first] = kv.second;
    }
    init(merged);
}

RDKafkaSubscriber::~RDKafkaSubscriber()
{
    deinit();
}

void RDKafkaSubscriber::init(const k2eg::common::MapStrKV& overrides)
{
    std::string errstr;
    // Defaults first, then overrides
    std::vector<std::pair<std::string, std::string>> defaults = {
        {"enable.partition.eof", "false"},
        {"bootstrap.servers", configuration->server_address},
        {"group.id", configuration->group_id.empty() ? UUID::generateUUIDLite() : configuration->group_id},
        {"client.id", std::string("k2eg_consumer_") + UUID::generateUUIDLite()},
        {"enable.auto.commit", "false"},
        {"enable.auto.offset.store", "true"},
        {"auto.offset.reset", "latest"},
        {"partition.assignment.strategy", "cooperative-sticky"},
    };
    applyDefaultsThenOverrides(defaults, overrides);
    // Topic conf pointer must be set explicitly
    RDK_CONF_SET(conf, "default_topic_conf", t_conf.get())

    // RDK_CONF_SET(conf, "batch.size", "2000", errstr);
    consumer.reset(RdKafka::KafkaConsumer::create(conf.get(), errstr));
    if (!consumer)
    {
        throw std::runtime_error("Error creating kafka producer (" + errstr + ")");
    }
}

void RDKafkaSubscriber::deinit()
{
    consumer->close();
}

void RDKafkaSubscriber::setQueue(const k2eg::common::StringVector& queue)
{
    if (!consumer)
    {
        throw std::runtime_error("Subscriber has not been initialized");
    }
    // replace actual topics
    topics = queue;
    RdKafka::ErrorCode err = consumer->subscribe(topics);
    if (err != RdKafka::ERR_NO_ERROR)
    {
        throw std::runtime_error("Error creating kafka producer (" + RdKafka::err2str(err) + ")");
    }
}

bool RDKafkaSubscriber::waitForAssignment(int timeout_ms)
{
    if (!consumer)
        return false;

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    std::vector<RdKafka::TopicPartition*> partitions;
    while (std::chrono::steady_clock::now() < deadline)
    {
        // Drive the internal librdkafka state machine by polling with a short timeout
        // This helps completing the group join and partition assignment.
        // Drive librdkafka internal state (group join / rebalance callbacks) without
        // actually consuming messages off the application queue.
        consumer->poll(10);

        auto err = consumer->assignment(partitions);
        if (err == RdKafka::ERR_NO_ERROR && !partitions.empty())
        {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return false;
}

void RDKafkaSubscriber::addQueue(const k2eg::common::StringVector& queue)
{
    if (!consumer)
    {
        throw std::runtime_error("Subscriber has not been initialized");
    }
    // replace actual topics
    topics.insert(std::end(topics), std::begin(queue), std::end(queue));
    RdKafka::ErrorCode err = consumer->subscribe(topics);
    if (err != RdKafka::ERR_NO_ERROR)
    {
        throw std::runtime_error("Error creating kafka producer (" + RdKafka::err2str(err) + ")");
    }
}

void RDKafkaSubscriber::commit(const bool& async)
{
    RdKafka::ErrorCode err = async ? consumer->commitSync() : consumer->commitAsync();
    if (err != RdKafka::ERR_NO_ERROR)
    {
        throw std::runtime_error("Error committing offset (" + RdKafka::err2str(err) + ")");
    }
    return;
}

void RDKafkaSubscriber::commit(const std::shared_ptr<const void>& handle, const bool& async)
{
    if (!handle)
        return;
    // Recreate a shared_ptr<CommitHandle> that shares ownership with the opaque handle
    using CH = SubscriberInterfaceElement::CommitHandle;
    auto alias = std::shared_ptr<CH>(handle, const_cast<CH*>(reinterpret_cast<const CH*>(handle.get())));
    if (!alias)
        return;

    RdKafka::TopicPartition*              tp = RdKafka::TopicPartition::create(alias->topic, alias->partition, alias->offset_to_commit);
    std::vector<RdKafka::TopicPartition*> offsets{tp};
    RdKafka::ErrorCode                    err = async ? consumer->commitAsync(offsets) : consumer->commitSync(offsets);
    delete tp;
    if (err != RdKafka::ERR_NO_ERROR)
    {
        throw std::runtime_error("Error committing offset (" + RdKafka::err2str(err) + ")");
    }

    // On successful commit, execute the optional on_commit_action
    try
    {
        if (alias->on_commit_action)
        {
            alias->on_commit_action();
        }
    }
    catch (const std::exception& ex)
    {
        // Log to cerr; avoid depending on logger from this module
        std::cerr << "RDKafkaSubscriber on_commit_action exception: " << ex.what() << std::endl;
    }
}

int RDKafkaSubscriber::getMsg(SubscriberInterfaceElementVector& messages, unsigned int m_num, unsigned int timeo)
{
    RdKafka::ErrorCode err = RdKafka::ERR_NO_ERROR;
    // RDK_CONS_APP_ << "Entering getMsg";
    bool looping = true;

    auto timeout_ms = std::chrono::milliseconds(timeo);
    auto end = std::chrono::system_clock::now() + std::chrono::milliseconds(timeo);
    while (messages.size() < m_num && looping)
    {
        std::unique_ptr<RdKafka::Message> msg(consumer->consume((int)timeout_ms.count()));
        if (!msg)
            continue;
        switch (msg->err())
        {
        case RdKafka::ERR__PARTITION_EOF:
            {
                // If partition EOF and have consumed messages, retry with timeout 1
                // This allows getting ready messages, while not waiting for new ones
                if (messages.size() > 0)
                {
                    timeout_ms = std::chrono::milliseconds(1);
                }
                msg.reset();
                break;
            }
        case RdKafka::ERR_NO_ERROR:
            {
                if (internalConsume(std::move(msg), messages) != 0)
                {
                    // RDK_CONS_ERR_ << "Error consuming received message";
                }
                break;
            }
        case RdKafka::ERR__TIMED_OUT:
        case RdKafka::ERR__TIMED_OUT_QUEUE:
            // Break of the loop if we timed out
            err = RdKafka::ERR_NO_ERROR;
            looping = false;
            msg.reset();
            break;
        default:
            // Set the error for any other errors and break
            if (messages.size() != 0)
            {
                err = msg->err();
            }
            msg.reset();
            looping = false;
            break;
        }
        // auto _now = ;
        // timeout_ms = std::chrono::duration_cast<std::chrono::microseconds>(end - _now).count();
        auto difference = (end - std::chrono::system_clock::now());
        if (difference.count() < 0)
        {
            break;
        }
    }
    // RDK_CONS_APP_ << "Exit getMsg";
    if (err != RdKafka::ERR_NO_ERROR)
    {
        // RDK_CONS_ERR_ << RdKafka::err2str(err);
        return -1;
    }
    else
    {
        return 0;
    }
}

int RDKafkaSubscriber::internalConsume(std::unique_ptr<RdKafka::Message> message, SubscriberInterfaceElementVector& messages)
{
    size_t                  len = message->len();
    std::unique_ptr<char[]> buffer(new char[len]);
    // copy message
    std::memcpy(buffer.get(), message->payload(), len);
    // read the headers
    SubscriberHeaders headers;
    if (message->headers())
    {
        auto all = message->headers()->get_all();
        for (auto& h : all)
        {
            // Header value may be null (tombstone) in Kafka; guard it
            if (h.value() != nullptr && h.value_size() > 0)
            {
                std::string value(static_cast<const char*>(h.value()), h.value_size());
                headers.insert(SubscriberHeadersPair(h.key(), value));
            }
            else
            {
                headers.insert(SubscriberHeadersPair(h.key(), std::string{}));
            }
        }
    }

    // Prepare commit info for this message so callers can commit only this message later
    auto commit_info = std::make_shared<SubscriberInterfaceElement::CommitHandle>();
    if (message->err() == RdKafka::ERR_NO_ERROR)
    {
        commit_info->topic = message->topic_name();
        commit_info->partition = message->partition();
        commit_info->offset_to_commit = message->offset() + 1; // next offset
    }

    auto element = std::make_shared<const SubscriberInterfaceElement>(
        SubscriberInterfaceElement{std::move(headers), message->key() == nullptr ? "default-key" : *message->key(), len, std::move(buffer)});
    // Attach the opaque commit handle
    const_cast<SubscriberInterfaceElement*>(element.get())->commit_handle = commit_info;
    messages.push_back(element);
    return 0;
}
