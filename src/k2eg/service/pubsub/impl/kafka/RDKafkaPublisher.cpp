#include <k2eg/service/pubsub/impl/kafka/RDKafkaPublisher.h>

#include <iostream>

using namespace k2eg::service::pubsub::impl::kafka;

RDKafkaPublisher::RDKafkaPublisher(ConstPublisherConfigurationUPtr configuration)
    : IPublisher(std::move(configuration))
    , RDKafkaBase()
    , _stop_inner_thread(false)
    , _auto_poll(false){init();}

RDKafkaPublisher::~RDKafkaPublisher() {deinit();}

void RDKafkaPublisher::setAutoPoll(bool autopoll) { _auto_poll = autopoll; }

void RDKafkaPublisher::dr_cb(RdKafka::Message& message) {
    // re-gain ownership of the allcoated message release in push methods
    PublishMessageUniquePtr message_managed(
        static_cast<PublishMessage*>(message.msg_opaque()));
    //if (!message_managed) return;
    // check if we have a callback
    EventCallback cb_handler = eventCallbackForReqType[message_managed->getReqType()];

    switch (message.status()) {
    case RdKafka::Message::MSG_STATUS_NOT_PERSISTED:
        if(cb_handler)cb_handler(OnError, message_managed.get());
        break;
    case RdKafka::Message::MSG_STATUS_POSSIBLY_PERSISTED:
        if(cb_handler)cb_handler(OnError, message_managed.get());
        break;
    case RdKafka::Message::MSG_STATUS_PERSISTED:
        if(cb_handler)cb_handler(OnSent, message_managed.get());
        break;
    default:
        break;
    }
    if (message.status() != RdKafka::Message::MSG_STATUS_PERSISTED) {
        //std::cerr << message.errstr() << std::flush;
    }
}

void RDKafkaPublisher::init() {
    std::string errstr;
    // RDK_CONF_SET(conf, "debug", "cgrp,topic,fetch,protocol", RDK_PUB_ERR_)
    RDK_CONF_SET(conf, "bootstrap.servers", configuration->server_address.c_str())
    RDK_CONF_SET(conf, "compression.type", "snappy")
    RDK_CONF_SET(conf, "linger.ms", "5")
    RDK_CONF_SET(conf, "dr_cb", this);
    producer.reset(RdKafka::Producer::create(conf.get(), errstr));
    if (!producer) {
        // RDK_PUB_ERR_ << "Failed to create producer: " << errstr << std::endl;
        throw std::runtime_error("Error creating kafka producer (" + errstr + ")");
    }
    // start polling thread
    if (_auto_poll) {
        auto_poll_thread = std::thread(&RDKafkaPublisher::autoPoll, this);
    }
}

void RDKafkaPublisher::autoPoll() {
    while (!this->_stop_inner_thread) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        flush(10);
    }
}

void RDKafkaPublisher::deinit() {
    producer->flush(100);
    if (_auto_poll) {
        _stop_inner_thread = true;
        auto_poll_thread.join();
    }
}

int RDKafkaPublisher::flush(const int timeo) {
    // RDK_PUB_DBG_ << "Flushing... ";
    while (producer->outq_len() > 0) {
        producer->poll(timeo);
    }
    // RDK_PUB_DBG_ << "Flushing...done ";
    return 0;
}

int RDKafkaPublisher::createQueue(const std::string& queue) {
    std::string errstr;
    std::unique_ptr<RdKafka::Topic> topic = nullptr;
    if (!queue.empty()) {
        topic = std::unique_ptr<RdKafka::Topic>(RdKafka::Topic::create(producer.get(), queue, t_conf.get(), errstr));
        if (!topic) {
            // RDK_PUB_ERR_ << "Failed to create topic: " << errstr;
            return -1;
        }
    }
    
    return 0;
}

int RDKafkaPublisher::pushMessage(PublishMessageUniquePtr message) {
    RdKafka::ErrorCode resp = RdKafka::ERR_NO_ERROR;
    RdKafka::Headers* headers = RdKafka::Headers::create();
    const std::string distribution_key = message->getDistributionKey();
    // headers->add("packet_num", std::to_string(idx));
    resp = producer->produce(message->getQueue(),
                             RdKafka::Topic::PARTITION_UA,
                             0 /* zero copy management */,
                             /* Value */
                             (void*)message->getBufferPtr(),
                             message->getBufferSize(),
                             /* Key */
                             distribution_key.c_str(),
                             distribution_key.size(),
                             /* Timestamp (defaults to now) */
                             0,
                             /* Message headers, if any */
                             headers,
                             /* pass PublishMessage instance to opaque information */
                             message.get());
    if (resp != RdKafka::ERR_NO_ERROR) {
        std::cerr << RdKafka::err2str(resp) << std::endl;
        // RDK_PUB_ERR_ << "Producer failed: " << ;
        delete headers; /* Headers are automatically deleted on produce() success. */
        return -1;
    } else {
        // whe need to release the message memory becaus is not more owned by this
        // instance
        message.release();
        return 0;
    }
}

int RDKafkaPublisher::pushMessages(PublisherMessageVector& messages) {
    int err = 0;
    auto message = messages.begin();

    while (message != messages.end()) {
        if (!(err = pushMessage(std::move(*message))))
            message = messages.erase(message);
        else
            break;
    }
    return !err ? 0 : -1;
}

size_t RDKafkaPublisher::getQueueMessageSize() { return 0; }
