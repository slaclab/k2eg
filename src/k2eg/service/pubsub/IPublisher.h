#ifndef IPUBLISHER_H
#define IPUBLISHER_H

#include <k2eg/common/types.h>

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <any>

#pragma once

namespace k2eg::service::pubsub {

/**
 * @brief Publisher configuration parameters.
 */
struct PublisherConfiguration
{
    /** @brief Publisher broker address (host:port). */
    std::string server_address;
    /** @brief Default flush timeout in milliseconds. */
    size_t flush_timeout_ms = 500;
    /**
     * @brief Implementation-specific overrides.
     *
     * Keys and values are implementation-defined; values are stored as
     * scalar types in std::any (commonly std::string, integers, bool).
     */
    k2eg::common::MapStrKV custom_impl_parameter;
};
DEFINE_PTR_TYPES(PublisherConfiguration)

/**
 * @brief Handle for a publishable message buffer.
 *
 * Implementations own the underlying buffer and routing metadata.
 */
class PublishMessage
{
public:
    virtual ~PublishMessage() {}

    virtual char*              getBufferPtr() = 0;
    virtual const size_t       getBufferSize() = 0;
    virtual const std::string& getQueue() = 0;
    virtual const std::string& getDistributionKey() = 0;
    virtual const std::string& getReqType() = 0;
};

typedef std::unique_ptr<PublishMessage>      PublishMessageUniquePtr;
typedef std::shared_ptr<PublishMessage>      PublishMessageSharedPtr;
typedef std::vector<PublishMessageUniquePtr> PublisherMessageVector;

typedef enum EventType
{
    OnDelivery,
    OnSent,
    OnError
} EventType;

// Callback called after the message has been sent
/**
 * @brief Callback invoked on publisher events.
 * @param EventType event type
 * @param PublishMessage* const message associated with the event
 * @param error_message optional error description when EventType::OnError
 */
typedef std::function<void(EventType, PublishMessage* const, const std::string& error_message)> EventCallback;
typedef std::map<std::string, EventCallback>                                                    MapEvtHndlrForReqType;
typedef std::pair<std::string, EventCallback>        MapEvtHndlrForReqTypePair;
typedef std::map<std::string_view, std::string_view> PublisherHeaders;

/**
 * @brief Queue (topic) creation parameters.
 */
struct QueueDescription
{
    std::string name;
    /** @brief Number of partitions. */
    long paritions;
    /** @brief Replication factor. */
    long replicas;
    /** @brief Retention time in milliseconds. */
    long retention_time;
    /** @brief Retention size in bytes. */
    long retention_size;
};
DEFINE_PTR_TYPES(QueueDescription)

/**
 * @brief Information about a single queue subscriber.
 */
struct QueueSubscriberInfo
{
    std::string client_id;
    std::string member_id;
    std::string host;
};
DEFINE_PTR_TYPES(QueueSubscriberInfo)

/**
 * @brief Group information containing subscribers.
 */
struct QueueSubscriberGroupInfo
{
    std::string                          name;
    std::vector<QueueSubscriberInfoUPtr> subscribers;
};
DEFINE_PTR_TYPES(QueueSubscriberGroupInfo)

/**
 * @brief Queue metadata snapshot.
 */
struct QueueMetadata
{
    /** @brief Queue name. */
    std::string                               name;
    /** @brief Groups subscribed to the queue. */
    std::vector<QueueSubscriberGroupInfoUPtr> subscriber_groups;
};
DEFINE_PTR_TYPES(QueueMetadata)

/**
 * @brief Abstract publisher interface.
 *
 * Implementations should honor configuration and optional runtime overrides.
 * Thread-safety is implementation-defined unless otherwise documented.
 */
class IPublisher
{
protected:
    MapEvtHndlrForReqType                 eventCallbackForReqType;
    const ConstPublisherConfigurationShrdPtr configuration;
    std::unordered_map<std::string, std::any> runtime_overrides_;

public:
    /**
     * @brief Construct with configuration.
     * @param configuration shared immutable publisher configuration
     */
    IPublisher(ConstPublisherConfigurationShrdPtr configuration);
    /**
     * @brief Construct with configuration and runtime overrides.
     * @param configuration shared immutable publisher configuration
     * @param overrides implementation-specific runtime overrides
     */
    IPublisher(ConstPublisherConfigurationShrdPtr configuration, const std::unordered_map<std::string, std::any>& overrides);
    IPublisher() = delete;
    IPublisher(const IPublisher&) = delete;
    IPublisher& operator=(const IPublisher&) = delete;
    virtual ~IPublisher() = default;
    /**
     * @brief Access provided runtime overrides.
     */
    const std::unordered_map<std::string, std::any>& getRuntimeOverrides() const { return runtime_overrides_; }
    /** @brief Enable/disable internal auto-poll behavior if supported. */
    virtual void setAutoPoll(bool autopoll) = 0;
    /** @brief Register per-request-type event callback. */
    virtual int               setCallBackForReqType(const std::string req_type, EventCallback eventCallback);
    /** @brief Create a queue (topic) with provided description. */
    virtual int               createQueue(const QueueDescription& new_queue) = 0;
    /** @brief Delete a queue (topic). */
    virtual int               deleteQueue(const std::string& queue_name) = 0;
    /** @brief Retrieve queue metadata, or nullptr on failure. */
    virtual QueueMetadataUPtr getQueueMetadata(const std::string& queue_name) = 0;
    /** @brief Flush pending messages; timeo in milliseconds. */
    virtual int               flush(const int timeo) = 0;
    /** @brief Publish a single message with optional headers. */
    virtual int pushMessage(PublishMessageUniquePtr message, const PublisherHeaders& headers = PublisherHeaders()) = 0;
    /** @brief Publish a batch of messages with optional headers. */
    virtual int pushMessages(PublisherMessageVector& messages, const PublisherHeaders& headers = PublisherHeaders()) = 0;
    /** @brief Get current internal queue size (implementation-defined). */
    virtual size_t getQueueMessageSize() = 0;
};
DEFINE_PTR_TYPES(IPublisher)
} // namespace k2eg::service::pubsub

#endif
