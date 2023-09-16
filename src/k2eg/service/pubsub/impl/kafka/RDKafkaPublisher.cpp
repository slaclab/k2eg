#include <k2eg/service/pubsub/impl/kafka/RDKafkaPublisher.h>

#include <iostream>
#include <string>
#include <librdkafka/rdkafkacpp.h>
#include <librdkafka/rdkafka.h>
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

    if(configuration->custom_impl_parameter.size()>0) {
        // apply custom user configuration
        applyCustomConfiguration(configuration->custom_impl_parameter);
    }

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

int RDKafkaPublisher::createQueue(const QueueDescription& new_queue) {
    std::string errstr;
    auto topic_configuration = RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC);
    if (new_queue.name.empty()) {
        throw std::runtime_error("The topic name is mandatory");
    }
    // rd_kafka_topic_new(rd_kafka_t *rk, const char *topic, rd_kafka_topic_conf_t *conf)
    //RDK_CONF_SET(topic_configuration, "retention.bytes", std::to_string(new_queue.retention_size));
    // RDK_CONF_SET(topic_configuration, "segment.ms", std::to_string(new_queue.retention_time));
    //RDK_CONF_SET(topic_configuration, "num.partitions", "2");
    auto topic = std::unique_ptr<RdKafka::Topic>(RdKafka::Topic::create(producer.get(), new_queue.name, t_conf.get(), errstr));
    if (!producer) {
        throw std::runtime_error("Error creating kafka producer (" + errstr + ")");
    }
    
    // Poll and wait for topic creation (you can customize this loop)
    bool topicCreated = false;
    RdKafka::ErrorCode err = RdKafka::ERR_NO_ERROR;
    while (!topicCreated) {
        // producer->poll(1000); // Poll for events every 1 second
        // Check if the topic exists in the metadata
        RdKafka::Metadata *metadata = nullptr;
        if (err = producer->metadata(false, NULL, &metadata, 1000); err == RdKafka::ERR_NO_ERROR) {
            std::unique_ptr<RdKafka::Metadata> metadata_uptr(metadata);
            // check for metadata
            const RdKafka::Metadata::TopicMetadataVector *topics = metadata_uptr->topics();
            for (const auto &topic : *topics) {
                if (topic->topic() == new_queue.name) {
                    topicCreated = true;
                    break;
                }
            }
        }
    }
    return 0;
}

int RDKafkaPublisher::pushMessage(PublishMessageUniquePtr message, const std::map<std::string,std::string>& headers) {
    RdKafka::ErrorCode resp = RdKafka::ERR_NO_ERROR;
    RdKafka::Headers* kafka_headers = RdKafka::Headers::create();
    for(auto& kv: headers) {
        kafka_headers->add(kv.first, kv.second);
    }
    const std::string distribution_key = message->getDistributionKey();
    auto msg_ptr = message.release();
    resp = producer->produce(msg_ptr->getQueue(),
                             RdKafka::Topic::PARTITION_UA,
                             0 /* zero copy management */,
                             /* Value */
                             (void*)msg_ptr->getBufferPtr(),
                             msg_ptr->getBufferSize(),
                             /* Key */
                             distribution_key.c_str(),
                             distribution_key.size(),
                             /* Timestamp (defaults to now) */
                             0,
                             /* Message headers, if any */
                             kafka_headers,
                             /* pass PublishMessage instance to opaque information */
                             msg_ptr);
    if (resp != RdKafka::ERR_NO_ERROR) {
        std::cerr << RdKafka::err2str(resp) << std::endl;
        /* Headers are automatically deleted on produce() success. */
        delete kafka_headers;
        delete msg_ptr;
        return -1;
    } else {
        // whe need to release the message memory becaus is not more owned by this
        // instance
        return 0;
    }
}

int RDKafkaPublisher::pushMessages(PublisherMessageVector& messages, const std::map<std::string,std::string>& headers) {
    int err = 0;
    auto message = messages.begin();

    while (message != messages.end()) {
        if (!(err = pushMessage(std::move(*message), headers)))
            message = messages.erase(message);
        else
            break;
    }
    return !err ? 0 : -1;
}

size_t RDKafkaPublisher::getQueueMessageSize() { return 0; }
