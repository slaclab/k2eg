#ifndef IPUBLISHER_H
#define IPUBLISHER_H

#include <k2eg/common/types.h>

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#pragma once
namespace k2eg::service::pubsub {

// publisher configuration
struct PublisherConfiguration {
    // publsher broker address
    std::string server_address;
    // custom k/v string map for implementation paramter
    k2eg::common::MapStrKV custom_impl_parameter;
};
DEFINE_PTR_TYPES(PublisherConfiguration)

/*
 * Message publish interface *implementation need to internally manage the implmenetaion instance
 */
class PublishMessage {
public:
    virtual ~PublishMessage() {}
    virtual char* getBufferPtr() = 0;
    virtual const size_t getBufferSize() = 0;
    virtual const std::string& getQueue() = 0;
    virtual const std::string& getDistributionKey() = 0;
    virtual const std::string& getReqType() = 0;
};

typedef std::unique_ptr<PublishMessage> PublishMessageUniquePtr;
typedef std::shared_ptr<PublishMessage> PublishMessageSharedPtr;
typedef std::vector<PublishMessageUniquePtr> PublisherMessageVector;

typedef enum EventType { OnDelivery, OnSent, OnError } EventType;

// Callback called after the message has been sent
typedef std::function<void(EventType, PublishMessage* const)> EventCallback;
typedef std::map<std::string, EventCallback> MapEvtHndlrForReqType;
typedef std::pair<std::string, EventCallback> MapEvtHndlrForReqTypePair;
typedef std::map<std::string,std::string> PublisherHeaders;

/*
Define the porperties of a queue
*/
struct QueueDescription {
    std::string name;
    // ow many partitions the topic need to have
    long paritions;
    // number of the replicas for the topics
    long replicas;
    // express in milliseconds
    long retention_time;
    // express in byte
    long retention_size;
};
DEFINE_PTR_TYPES(QueueDescription)


struct QueueSubscriberGroupInfo{
    std::string subscriber_group_name;
    std::vector<std::string> subscriber_names;
};

/*
Define the queue metadata infromation
*/
struct QueueMetadata{
    // the number of subcriber to the queue
    size_t subscribe_count;
    std::vector<QueueSubscriberGroupInfo> subscriber_groups;
};
DEFINE_PTR_TYPES(QueueMetadata)

class IPublisher {
protected:
    MapEvtHndlrForReqType eventCallbackForReqType;
    const ConstPublisherConfigurationUPtr configuration;
public:
    IPublisher(ConstPublisherConfigurationUPtr configuration);
    IPublisher() = delete;
    IPublisher(const IPublisher&) = delete;
    IPublisher& operator=(const IPublisher&) = delete;
    virtual ~IPublisher() = default;
    virtual void setAutoPoll(bool autopoll) = 0;
    //! PublisherInterface initialization
    virtual int setCallBackForReqType(const std::string req_type, EventCallback eventCallback);
    virtual int createQueue(const QueueDescription& new_queue) = 0;
    virtual int deleteQueue(const std::string& queue_name) = 0;
    virtual QueueMetadataUPtr getQueueMetadata(const std::string& queue_name) = 0;
    virtual int flush(const int timeo) = 0;
    virtual int pushMessage(PublishMessageUniquePtr message, const PublisherHeaders& headers = PublisherHeaders()) = 0;
    virtual int pushMessages(PublisherMessageVector& messages, const PublisherHeaders& headers = PublisherHeaders()) = 0;
    virtual size_t getQueueMessageSize() = 0;
};
DEFINE_PTR_TYPES(IPublisher)
} // namespace k2eg::service::pubsub

#endif