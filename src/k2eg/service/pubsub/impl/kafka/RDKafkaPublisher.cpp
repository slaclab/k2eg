#include <k2eg/service/pubsub/impl/kafka/RDKafkaPublisher.h>
#include <librdkafka/rdkafkacpp.h>

#include <cstddef>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

#include "k2eg/service/pubsub/IPublisher.h"
#include "librdkafka/rdkafka.h"

using namespace k2eg::service::pubsub;
using namespace k2eg::service::pubsub::impl::kafka;

RDKafkaPublisher::RDKafkaPublisher(ConstPublisherConfigurationUPtr configuration)
    : IPublisher(std::move(configuration)), RDKafkaBase(), _stop_inner_thread(false), _auto_poll(true) {
  init();
}

RDKafkaPublisher::~RDKafkaPublisher() { deinit(); }

void
RDKafkaPublisher::setAutoPoll(bool autopoll) {
  _auto_poll = autopoll;
}

void
RDKafkaPublisher::dr_cb(RdKafka::Message &message) {
  // re-gain ownership of the allcoated message release in push methods
  PublishMessageUniquePtr message_managed(static_cast<PublishMessage *>(message.msg_opaque()));
  // if (!message_managed) return;
  //  check if we have a callback
  EventCallback cb_handler = eventCallbackForReqType[message_managed->getReqType()];

  switch (message.status()) {
    case RdKafka::Message::MSG_STATUS_NOT_PERSISTED:
      if (cb_handler) cb_handler(OnError, message_managed.get(), message.errstr());
      break;
    case RdKafka::Message::MSG_STATUS_POSSIBLY_PERSISTED:
      if (cb_handler) cb_handler(OnError, message_managed.get(), message.errstr());
      break;
    case RdKafka::Message::MSG_STATUS_PERSISTED:
      if (cb_handler) cb_handler(OnSent, message_managed.get(), "");
      break;
    default: break;
  }
}

void
RDKafkaPublisher::init() {
  std::string errstr;
  // RDK_CONF_SET(conf, "debug", "cgrp,topic,fetch,protocol", RDK_PUB_ERR_)
  RDK_CONF_SET(conf, "bootstrap.servers", configuration->server_address.c_str())
  RDK_CONF_SET(conf, "compression.type", "snappy")
  //RDK_CONF_SET(conf, "linger.ms", "5")
  //RDK_CONF_SET(conf, "batch.size", "1048576")
  
  RDK_CONF_SET(conf, "dr_cb", this);

  if (configuration->custom_impl_parameter.size() > 0) {
    // apply custom user configuration
    applyCustomConfiguration(configuration->custom_impl_parameter);
  }

  producer.reset(RdKafka::Producer::create(conf.get(), errstr));
  if (!producer) {
    // RDK_PUB_ERR_ << "Failed to create producer: " << errstr << std::endl;
    throw std::runtime_error("Error creating kafka producer (" + errstr + ")");
  }
  // start polling thread
  if (_auto_poll) { auto_poll_thread = std::thread(&RDKafkaPublisher::autoPoll, this); }
}

void
RDKafkaPublisher::autoPoll() {
  while (!this->_stop_inner_thread) {
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    flush(10);
  }
}

void
RDKafkaPublisher::deinit() {
  int retry = 100;
  while (producer->outq_len() > 0 && retry>0) { producer->poll(1000); retry--;}
  if (_auto_poll) {
    _stop_inner_thread = true;
    auto_poll_thread.join();
  }
}

int
RDKafkaPublisher::flush(const int timeo) {
  //int retry = 3;
  // RDK_PUB_DBG_ << "Flushing... ";
  //while (producer->outq_len() > 0 && retry>0) { producer->poll(timeo); retry--;}
  producer->poll(timeo);
  // RDK_PUB_DBG_ << "Flushing...done ";
  return 0;
}

#define DEFAULT_TO_1(val) val == 0 ? 1 : val
int
RDKafkaPublisher::createQueue(const QueueDescription &new_queue) {
  const int                             errstr_cnt = 512;
  char                                  errstr[errstr_cnt];
  const int                             opt_timeout = 10000;
  rd_kafka_resp_err_t                   err         = RD_KAFKA_RESP_ERR_NO_ERROR;
  const rd_kafka_CreateTopics_result_t *res         = nullptr;
  const rd_kafka_topic_result_t       **restopics   = nullptr;

  std::unique_ptr<rd_kafka_NewTopic_t *, RdKafkaNewTopicArrayDeleter> new_topics_uptr(
      static_cast<rd_kafka_NewTopic_t **>(malloc(sizeof(rd_kafka_NewTopic_t *) * 1)), RdKafkaNewTopicArrayDeleter(1));
  // define the topic
  new_topics_uptr.get()[0] =
      rd_kafka_NewTopic_new(new_queue.name.c_str(), DEFAULT_TO_1(new_queue.paritions), DEFAULT_TO_1(new_queue.replicas), errstr, errstr_cnt);

  /*
   * Add various configuration properties
   */
  if (new_queue.retention_time) {
    err = rd_kafka_NewTopic_set_config(new_topics_uptr.get()[0], "retention.ms", std::to_string(new_queue.retention_time).c_str());
    if (err) { throw std::runtime_error("Error configuring retention ms for topic (" + std::string(rd_kafka_err2str(err)) + ")"); }
  }
  if (new_queue.retention_size) {
    err = rd_kafka_NewTopic_set_config(new_topics_uptr.get()[0], "retention.bytes", std::to_string(new_queue.retention_size).c_str());
    if (err) { throw std::runtime_error("Error configuring retention byte for topic (" + std::string(rd_kafka_err2str(err)) + ")"); }
  }

  // set the timeout on the request
  std::unique_ptr<rd_kafka_AdminOptions_t, RdKafkaAdminOptionDeleter> admin_options(
      rd_kafka_AdminOptions_new(producer.get()->c_ptr(), RD_KAFKA_ADMIN_OP_DELETETOPICS), RdKafkaAdminOptionDeleter());

  if (err = rd_kafka_AdminOptions_set_request_timeout(admin_options.get(), opt_timeout, errstr, sizeof(errstr)); err != RD_KAFKA_RESP_ERR_NO_ERROR) {
    return -1;
  }

  if (err = rd_kafka_AdminOptions_set_operation_timeout(admin_options.get(), opt_timeout - 5000, errstr, sizeof(errstr)); err != RD_KAFKA_RESP_ERR_NO_ERROR) {
    return -2;
  }

  // allocate queue
  std::unique_ptr<rd_kafka_queue_t, RdKafkaQueueDeleter> queue(rd_kafka_queue_new(producer.get()->c_ptr()), RdKafkaQueueDeleter());
  // create topic
  rd_kafka_CreateTopics(producer.get()->c_ptr(), new_topics_uptr.get(), 1, admin_options.get(), queue.get());
  // wait for completion
  std::unique_ptr<rd_kafka_event_t, RdKafkaEventDeleter> event_uptr;
  do {
    event_uptr.reset(rd_kafka_queue_poll(queue.get(), 1.0));
    if (!event_uptr) continue;
    const char *evt_name = rd_kafka_event_name(event_uptr.get());
    if (rd_kafka_event_error(event_uptr.get())) {
      throw std::runtime_error("Error creating topic topic (" + std::string(rd_kafka_event_error_string(event_uptr.get())) + ")");
    }
  } while (rd_kafka_event_type(event_uptr.get()) != RD_KAFKA_EVENT_CREATETOPICS_RESULT);

  // get event results
  res = rd_kafka_event_CreateTopics_result(event_uptr.get());
  if (err) { throw std::runtime_error("Error creating topic topic (" + std::string(rd_kafka_err2str(err)) + ")"); }

  // exstract result information by event result
  int    result = 0;
  size_t topic_count;
  restopics = rd_kafka_CreateTopics_result_topics(res, &topic_count);
  /* Scan topics for proper fields and expected failures. */
  for (size_t i = 0; i < (int)topic_count; i++) {
    const rd_kafka_topic_result_t *tres               = restopics[i];
    auto                           topic_name         = std::string(rd_kafka_topic_result_name(tres));
    auto                           toipc_result_error = rd_kafka_topic_result_error(tres);
    if (toipc_result_error != RD_KAFKA_RESP_ERR_NO_ERROR && toipc_result_error != RD_KAFKA_RESP_ERR_TOPIC_ALREADY_EXISTS) {
      auto topic_result_error_string = std::string(rd_kafka_topic_result_error_string(tres));
      auto err_name                  = std::string(rd_kafka_err2name(toipc_result_error));
      throw std::runtime_error("Error '" + err_name + "' creating topic " + topic_name + " (" + topic_result_error_string + ")");
    }
  }
  return 0;
}

int
RDKafkaPublisher::deleteQueue(const std::string &queue_name) {
  char                errstr[512];
  size_t              res_cnt;
  const int           tmout = 30 * 1000;
  rd_kafka_resp_err_t err;

  std::unique_ptr<rd_kafka_DeleteTopic_t *, RdKafkaDeleteTopicArrayDeleter> del_topics_uptr(
      static_cast<rd_kafka_DeleteTopic_t **>(malloc(sizeof(rd_kafka_DeleteTopic_t *) * 1)), RdKafkaDeleteTopicArrayDeleter(1));
  del_topics_uptr.get()[0] = rd_kafka_DeleteTopic_new(queue_name.c_str());

  std::unique_ptr<rd_kafka_AdminOptions_t, RdKafkaAdminOptionDeleter> admin_options(
      rd_kafka_AdminOptions_new(producer.get()->c_ptr(), RD_KAFKA_ADMIN_OP_DELETETOPICS), RdKafkaAdminOptionDeleter());

  if (err = rd_kafka_AdminOptions_set_request_timeout(admin_options.get(), tmout, errstr, sizeof(errstr)); err != RD_KAFKA_RESP_ERR_NO_ERROR) {
    throw std::runtime_error("Error creating kafka option reqeust timeout (" + std::string(errstr) + ")");
  }

  if (err = rd_kafka_AdminOptions_set_operation_timeout(admin_options.get(), tmout - 5000, errstr, sizeof(errstr)); err != RD_KAFKA_RESP_ERR_NO_ERROR) {
    throw std::runtime_error("Error creating kafka option reqeust timeout (" + std::string(errstr) + ")");
  }

  // allocate queue
  std::unique_ptr<rd_kafka_queue_t, RdKafkaQueueDeleter> queue(rd_kafka_queue_new(producer.get()->c_ptr()), RdKafkaQueueDeleter());
  // delete topic
  rd_kafka_DeleteTopics(producer.get()->c_ptr(), del_topics_uptr.get(), 1, admin_options.get(), queue.get());

  std::unique_ptr<rd_kafka_event_t, RdKafkaEventDeleter> event_uptr;
  do {
    event_uptr.reset(rd_kafka_queue_poll(queue.get(), 1.0));
    if (!event_uptr) continue;
    const char *evt_name = rd_kafka_event_name(event_uptr.get());
    if (rd_kafka_event_error(event_uptr.get())) {
      throw std::runtime_error("Error deleting topic (" + std::string(rd_kafka_event_error_string(event_uptr.get())) + ")");
    }
  } while (rd_kafka_event_type(event_uptr.get()) != RD_KAFKA_EVENT_DELETETOPICS_RESULT);

  const rd_kafka_DeleteTopics_result_t *res  = rd_kafka_event_DeleteTopics_result(event_uptr.get());
  const rd_kafka_topic_result_t       **terr = rd_kafka_DeleteTopics_result_topics(res, &res_cnt);
  if (res_cnt != 1) {
    // no topic has been deleted
    return -3;
  }
  if (err = rd_kafka_topic_result_error(terr[0]); err != RD_KAFKA_RESP_ERR_NO_ERROR) {
    auto err_topic_name = std::string(rd_kafka_topic_result_name(terr[0]));
    auto err_str        = std::string(rd_kafka_topic_result_error_string(terr[0]));
    throw std::runtime_error("Error deleting topic "+err_topic_name+"(" + err_str + ")");
  }
  return 0;
}

QueueMetadataUPtr
RDKafkaPublisher::getQueueMetadata(const std::string &queue_name) {
  // create empty metadata ptr
  char errstr[512];
  rd_kafka_resp_err_t err = RD_KAFKA_RESP_ERR_NO_ERROR;
  // const rd_kafka_metadata_t *_metadata = nullptr;
  const int                  tmout    = 30 * 1000;
  QueueMetadataUPtr          result = std::make_unique<QueueMetadata>();
  result->name = queue_name;
  // allocate topic structure
  std::unique_ptr<rd_kafka_topic_t, RdKafkaTopicDeleter> topic_uptr(rd_kafka_topic_new(producer.get()->c_ptr(), queue_name.c_str(), NULL),
                                                                    RdKafkaTopicDeleter());

  // consumer group
  std::unique_ptr<rd_kafka_queue_t, RdKafkaQueueDeleter>              queue(rd_kafka_queue_new(producer.get()->c_ptr()), RdKafkaQueueDeleter());
  std::unique_ptr<rd_kafka_AdminOptions_t, RdKafkaAdminOptionDeleter> admin_options(
      rd_kafka_AdminOptions_new(producer.get()->c_ptr(), RD_KAFKA_ADMIN_OP_LISTCONSUMERGROUPS), 
      RdKafkaAdminOptionDeleter()
      );

  if (err = rd_kafka_AdminOptions_set_request_timeout(admin_options.get(), tmout, errstr, sizeof(errstr)); err != RD_KAFKA_RESP_ERR_NO_ERROR) {
    // rd_kafka_metadata_destroy(metadata); // Deallocate metadata on error
    throw std::runtime_error("Error creating kafka option reqeust timeout (" + std::string(errstr) + ")");
  }

  rd_kafka_ListConsumerGroups(producer.get()->c_ptr(), admin_options.get(), queue.get());
  // rd_kafka_event_t *event = ;
  std::unique_ptr<rd_kafka_event_t, RdKafkaEventDeleter> event_uptr(rd_kafka_queue_poll(queue.get(), -1), RdKafkaEventDeleter());                                              
  if (!event_uptr) {
    /* see yield call in stop() signal handler */
    throw std::runtime_error("Null event received");
  } else if (rd_kafka_event_error(event_uptr.get())) {
    rd_kafka_resp_err_t err = rd_kafka_event_error(event_uptr.get());
    /* ListConsumerGroups request failed */
    // rd_kafka_metadata_destroy(metadata); // Deallocate metadata on error
    throw std::runtime_error("Error executing ListConsumerGroups (" + std::string(rd_kafka_event_error_string(event_uptr.get())) + ")");
  } else {
    /* ListConsumerGroups request succeeded, but individual
     * groups may have errors. */
    const rd_kafka_ListConsumerGroups_result_t *consumer_group_info = rd_kafka_event_ListConsumerGroups_result(event_uptr.get());
    scan_groups(consumer_group_info, *result);
  }
  return result;
}

/**
 * @brief Print group information.
 */
int
RDKafkaPublisher::scan_groups(const rd_kafka_ListConsumerGroups_result_t *list,  QueueMetadata& q_desc_ref) {
  size_t                                  i;
  const rd_kafka_ConsumerGroupListing_t **result_groups;
  const rd_kafka_error_t                **errors;
  size_t                                  result_groups_cnt;
  size_t                                  result_error_cnt;
  result_groups = rd_kafka_ListConsumerGroups_result_valid(list, &result_groups_cnt);
  errors        = rd_kafka_ListConsumerGroups_result_errors(list, &result_error_cnt);

  if (result_groups_cnt == 0) { return 0; }

  for (i = 0; i < result_groups_cnt; i++) {
    const rd_kafka_ConsumerGroupListing_t *group                    = result_groups[i];
    const char                            *group_id                 = rd_kafka_ConsumerGroupListing_group_id(group);
    rd_kafka_consumer_group_state_t        state                    = rd_kafka_ConsumerGroupListing_state(group);
    int                                    is_simple_consumer_group = rd_kafka_ConsumerGroupListing_is_simple_consumer_group(group);
    const struct rd_kafka_group_list      *grplistp                 = nullptr;

    auto group_info = get_group_info(group_id);
    if(group_info->subscribers.size()) {
      q_desc_ref.subscriber_groups.push_back(std::move(group_info));
    }
  }
  return 0;
}

QueueSubscriberGroupInfoUPtr
RDKafkaPublisher::get_group_info(const char *group) {
  rd_kafka_resp_err_t               err;
  const rd_kafka_group_list *grplist;
  QueueSubscriberGroupInfoUPtr result = std::make_unique<QueueSubscriberGroupInfo>();
  err = rd_kafka_list_groups(producer.get()->c_ptr(), group, &grplist, 10000);
  if (err) {
    throw std::runtime_error("Failed to acquire group list (" + std::string(rd_kafka_err2str(err)) + ")");
  }
  std::unique_ptr<const rd_kafka_group_list,  RdKafkaGroupListDeleter> grlist_uptr(grplist, RdKafkaGroupListDeleter());
  // set group name
  result->name = group;
  // scan the subscriber of the group
  for (int i = 0; i < grlist_uptr->group_cnt; i++) {
    const struct rd_kafka_group_info *gi = &grlist_uptr->groups[i];
    int                               j;
    if (gi->err) throw std::runtime_error("Failed to acquire group info (" + std::string(rd_kafka_err2str(gi->err)) + ")");

    for (j = 0; j < gi->member_cnt; j++) {
      const struct rd_kafka_group_member_info *mi;
      mi = &gi->members[j];
      result->subscribers.push_back(
        MakeQueueSubscriberInfoUPtr(
          QueueSubscriberInfo{
            .client_id = mi->client_id,
            .member_id = mi->member_id,
            .host = mi->client_host
          }
        )
      );
    }
  }
  return result;
}

int
RDKafkaPublisher::pushMessage(PublishMessageUniquePtr message, const std::map<std::string, std::string> &headers) {
  RdKafka::ErrorCode resp          = RdKafka::ERR_NO_ERROR;
  RdKafka::Headers  *kafka_headers = RdKafka::Headers::create();
  for (auto &kv : headers) { kafka_headers->add(kv.first, kv.second); }
  const std::string distribution_key = message->getDistributionKey();
  auto              msg_ptr          = message.release();
  resp                               = producer->produce(msg_ptr->getQueue(),
                           RdKafka::Topic::PARTITION_UA,
                           0 /* zero copy management */,
                           /* Value */
                           (void *)msg_ptr->getBufferPtr(),
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

int
RDKafkaPublisher::pushMessages(PublisherMessageVector &messages, const std::map<std::string, std::string> &headers) {
  int  err     = 0;
  auto message = messages.begin();

  while (message != messages.end()) {
    if (!(err = pushMessage(std::move(*message), headers)))
      message = messages.erase(message);
    else
      break;
  }
  return !err ? 0 : -1;
}

size_t
RDKafkaPublisher::getQueueMessageSize() {
  return 0;
}