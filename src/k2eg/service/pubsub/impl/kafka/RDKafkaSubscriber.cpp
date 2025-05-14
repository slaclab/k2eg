#include <k2eg/common/uuid.h>

#include <k2eg/service/pubsub/ISubscriber.h>
#include <k2eg/service/pubsub/impl/kafka/RDKafkaSubscriber.h>

#include <chrono>
#include <stdexcept>


using namespace k2eg::common;
using namespace k2eg::service::pubsub::impl::kafka;

RDKafkaSubscriber::RDKafkaSubscriber(ConstSubscriberConfigurationUPtr configuration) : ISubscriber(std::move(configuration)), RDKafkaBase() { init(); }

RDKafkaSubscriber::~RDKafkaSubscriber() { deinit(); }

void
RDKafkaSubscriber::init() {
  std::string errstr;
  // setting properties
  RDK_CONF_SET(conf, "enable.partition.eof", "false")
  // RDK_CONF_SET(conf, "debug", "cgrp,topic,fetch,protocol")
  RDK_CONF_SET(conf, "bootstrap.servers", configuration->server_address)
  RDK_CONF_SET(conf, "group.id", configuration->group_id.empty() ? UUID::generateUUIDLite() : configuration->group_id)
  RDK_CONF_SET(conf, "client.id", "k2eg_consumer_" + UUID::generateUUIDLite())
  RDK_CONF_SET(conf, "enable.auto.commit", "false")
  RDK_CONF_SET(conf, "enable.auto.offset.store", "true")
  RDK_CONF_SET(conf, "auto.offset.reset", "latest")
  RDK_CONF_SET(conf, "default_topic_conf", t_conf.get())

  if(configuration->custom_impl_parameter.size()>0) {
      // apply custom user configuration
      applyCustomConfiguration(configuration->custom_impl_parameter);
  }

  // RDK_CONF_SET(conf, "batch.size", "2000", errstr);
  consumer.reset(RdKafka::KafkaConsumer::create(conf.get(), errstr));
  if (!consumer) { throw std::runtime_error("Error creating kafka producer (" + errstr + ")"); }
}
void
RDKafkaSubscriber::deinit() {
  consumer->close();
}

void
RDKafkaSubscriber::setQueue(const k2eg::common::StringVector &queue) {
  if (!consumer) { throw std::runtime_error("Subscriber has not been initialized"); }
  // replace actual topics
  topics                 = queue;
  RdKafka::ErrorCode err = consumer->subscribe(topics);
  if (err != RdKafka::ERR_NO_ERROR) { throw std::runtime_error("Error creating kafka producer (" + RdKafka::err2str(err) + ")"); }
}

void
RDKafkaSubscriber::addQueue(const k2eg::common::StringVector &queue) {
  if (!consumer) { throw std::runtime_error("Subscriber has not been initialized"); }
  // replace actual topics
  topics.insert(std::end(topics), std::begin(queue), std::end(queue));
  RdKafka::ErrorCode err = consumer->subscribe(topics);
  if (err != RdKafka::ERR_NO_ERROR) { throw std::runtime_error("Error creating kafka producer (" + RdKafka::err2str(err) + ")"); }
}

void
RDKafkaSubscriber::commit(const bool &async) {
  RdKafka::ErrorCode err = async ? consumer->commitSync() : consumer->commitAsync();
  if (err != RdKafka::ERR_NO_ERROR) { throw std::runtime_error("Error committing offset (" + RdKafka::err2str(err) + ")"); }
}

int
RDKafkaSubscriber::getMsg(SubscriberInterfaceElementVector &messages, unsigned int m_num, unsigned int timeo) {
  RdKafka::ErrorCode err = RdKafka::ERR_NO_ERROR;
  // RDK_CONS_APP_ << "Entering getMsg";
  bool looping = true;

  auto timeout_ms = std::chrono::milliseconds(timeo);
  auto end        = std::chrono::system_clock::now() + std::chrono::milliseconds(timeo);
  while (messages.size() < m_num && looping) {
    std::unique_ptr<RdKafka::Message> msg(consumer->consume((int)timeout_ms.count()));
    if (!msg) continue;
    switch (msg->err()) {
      case RdKafka::ERR__PARTITION_EOF: {
        // If partition EOF and have consumed messages, retry with timeout 1
        // This allows getting ready messages, while not waiting for new ones
        if (messages.size() > 0) { timeout_ms = std::chrono::milliseconds(1); }
        msg.reset();
        break;
      }
      case RdKafka::ERR_NO_ERROR: {
        if (internalConsume(std::move(msg), messages) != 0) {
          // RDK_CONS_ERR_ << "Error consuming received message";
        }
        break;
      }
      case RdKafka::ERR__TIMED_OUT:
      case RdKafka::ERR__TIMED_OUT_QUEUE:
        // Break of the loop if we timed out
        err     = RdKafka::ERR_NO_ERROR;
        looping = false;
        msg.reset();
        break;
      default:
        // Set the error for any other errors and break
        if (messages.size() != 0) { err = msg->err(); }
        msg.reset();
        looping = false;
        break;
    }
    // auto _now = ;
    // timeout_ms = std::chrono::duration_cast<std::chrono::microseconds>(end - _now).count();
    auto difference = (end - std::chrono::system_clock::now());
    if (difference.count() < 0) { break; }
  }
  // RDK_CONS_APP_ << "Exit getMsg";
  if (err != RdKafka::ERR_NO_ERROR) {
    // RDK_CONS_ERR_ << RdKafka::err2str(err);
    return -1;
  } else {
    return 0;
  }
}

int
RDKafkaSubscriber::internalConsume(std::unique_ptr<RdKafka::Message> message, SubscriberInterfaceElementVector &messages) {
  size_t                  len = message->len();
  std::unique_ptr<char[]> buffer(new char[len]);
  // copy message
  std::memcpy(buffer.get(), message->payload(), len);
  // read the headers
  SubscriberHeaders headers;
  if (message->headers()) {
    for (auto &h : message->headers()->get_all()) {
      std::string value((const char *)h.value(), h.value_size());
      headers.insert(SubscriberHeadersPair(h.key(), value));
    }
  }

  messages.push_back(std::make_shared<const SubscriberInterfaceElement>(
      SubscriberInterfaceElement{std::move(headers), message->key() == nullptr ? "default-key" : *message->key(), len, std::move(buffer)}));
  return 0;
}
