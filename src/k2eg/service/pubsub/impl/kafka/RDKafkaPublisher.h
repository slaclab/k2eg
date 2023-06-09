#ifndef RDKKAFKAPUBLISHER_H
#define RDKKAFKAPUBLISHER_H

#pragma once
#include <thread>
#include <memory>

#include <librdkafka/rdkafkacpp.h>

#include <k2eg/service/pubsub/IPublisher.h>
#include <k2eg/service/pubsub/impl/kafka/RDKafkaBase.h>
namespace k2eg::service::pubsub::impl::kafka
{
    class RDKafkaPublisher : public IPublisher, RDKafkaBase, RdKafka::DeliveryReportCb
    {
        bool _stop_inner_thread;
        bool _auto_poll;
        std::thread auto_poll_thread;
        std::unique_ptr<RdKafka::Producer> producer;
    protected:
        void dr_cb(RdKafka::Message &message);
        void autoPoll();
        virtual void init();
        virtual void deinit();
    public:
        explicit RDKafkaPublisher(ConstPublisherConfigurationUPtr configuration);
        virtual ~RDKafkaPublisher();
        virtual int createQueue(const std::string &queue);
        virtual void setAutoPoll(bool autopoll);
        virtual int flush(const int timeo = 10000);
        virtual int pushMessage(PublishMessageUniquePtr message, const PublisherHeaders& headers = PublisherHeaders());
        virtual int pushMessages(PublisherMessageVector &messages, const PublisherHeaders& headers = PublisherHeaders());
        virtual size_t getQueueMessageSize();
    };
}

#endif