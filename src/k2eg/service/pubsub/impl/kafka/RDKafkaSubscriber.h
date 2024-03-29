#ifndef RDKAFKASUBSCRIBER_H
#define RDKAFKASUBSCRIBER_H

#pragma once

#include <memory>
#include <string>
#include <librdkafka/rdkafkacpp.h>

#include <k2eg/common/types.h>
#include <k2eg/service/pubsub/ISubscriber.h>
#include <k2eg/service/pubsub/impl/kafka/RDKafkaBase.h>

namespace k2eg::service::pubsub::impl::kafka
{
    class RDKafkaSubscriber : public ISubscriber, RDKafkaBase
    {
        std::unique_ptr<RdKafka::KafkaConsumer> consumer;
        k2eg::common::StringVector topics;
    protected:
        int internalConsume(std::unique_ptr<RdKafka::Message> message, SubscriberInterfaceElementVector &dataVector);
        virtual void init();
        virtual void deinit();
    public:
        RDKafkaSubscriber(ConstSubscriberConfigurationUPtr configuration);
        RDKafkaSubscriber() = delete;
        virtual ~RDKafkaSubscriber();
        virtual void setQueue(const k2eg::common::StringVector &queue);
        virtual void addQueue(const k2eg::common::StringVector &queue);
        virtual void commit(const bool& async = false);
        virtual int getMsg(SubscriberInterfaceElementVector &dataVector, unsigned int m_num, unsigned int timeo = 250);
    };

}
#endif