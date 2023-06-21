
#include <gtest/gtest.h>
#include <k2eg/common/ProgramOptions.h>
#include <k2eg/common/utility.h>
#include <k2eg/controller/command/cmd/Command.h>
#include <k2eg/controller/command/cmd/PutCommand.h>
#include <k2eg/controller/node/NodeController.h>
#include <k2eg/service/ServiceResolver.h>
#include <k2eg/service/data/DataStorage.h>
#include <k2eg/service/epics/EpicsData.h>
#include <k2eg/service/epics/EpicsServiceManager.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/log/impl/BoostLogger.h>
#include <k2eg/service/metric/impl/prometheus/PrometheusMetricService.h>
#include <k2eg/service/pubsub/pubsub.h>

#include <boost/json.hpp>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <latch>
#include <memory>
#include <msgpack.hpp>
#include <ostream>
#include <random>
#include <string>
#include <thread>

#include "boost/json/object.hpp"
#include "k2eg/service/metric/IMetricService.h"
#include "msgpack/v3/object_fwd_decl.hpp"

namespace bj = boost::json;

namespace fs = std::filesystem;

using namespace k2eg::common;

using namespace k2eg::controller::command;
using namespace k2eg::controller::command::cmd;
using namespace k2eg::controller::node;

using namespace k2eg::service;
using namespace k2eg::service::log;
using namespace k2eg::service::log::impl;
using namespace k2eg::service::pubsub;
using namespace k2eg::service::data;
using namespace k2eg::service::epics_impl;

using namespace k2eg::service::pubsub;
using namespace k2eg::service::pubsub::impl::kafka;

using namespace k2eg::service::metric;
using namespace k2eg::service::metric::impl::prometheus_impl;

#define KAFKA_HOSTNAME         "kafka:9092"
#define KAFKA_TOPIC_ACQUIRE_IN "acquire_commad_in"

int tcp_port = 9000;

class DummyPublisher : public IPublisher {
  std::latch& lref;

 public:
  std::vector<PublishMessageSharedPtr> sent_messages;
  DummyPublisher(std::latch& lref)
      : IPublisher(std::make_unique<const PublisherConfiguration>(PublisherConfiguration{.server_address = "fake_address"})), lref(lref){};
  ~DummyPublisher() = default;
  void
  setAutoPoll(bool autopoll) {}
  int
  setCallBackForReqType(const std::string req_type, EventCallback eventCallback) {
    return 0;
  }
  int
  createQueue(const std::string& queue) {
    return 0;
  }
  int
  flush(const int timeo) {
    return 0;
  }
  int
  pushMessage(PublishMessageUniquePtr message, const PublisherHeaders& header = PublisherHeaders()) {
    sent_messages.push_back(std::move(message));
    lref.count_down();
    return 0;
  }
  int
  pushMessages(PublisherMessageVector& messages, const PublisherHeaders& header = PublisherHeaders()) {
    for (auto& uptr : messages) {
      sent_messages.push_back(std::move(uptr));
      lref.count_down();
    }
    return 0;
  }
  size_t
  getQueueMessageSize() {
    return sent_messages.size();
  }
};

class DummyPublisherCounter : public IPublisher {
  std::uint64_t counter;

 public:
  std::latch l;
  DummyPublisherCounter(unsigned int latch_counter)
      : IPublisher(std::make_unique<const PublisherConfiguration>(PublisherConfiguration{.server_address = "fake_address"})), l(latch_counter), counter(0){};
  ~DummyPublisherCounter() = default;
  void
  setAutoPoll(bool autopoll) {}
  int
  setCallBackForReqType(const std::string req_type, EventCallback eventCallback) {
    return 0;
  }
  int
  createQueue(const std::string& queue) {
    return 0;
  }
  int
  flush(const int timeo) {
    return 0;
  }
  int
  pushMessage(PublishMessageUniquePtr message, const PublisherHeaders& header = PublisherHeaders()) {
    counter++;
    l.count_down();
    return 0;
  }
  int
  pushMessages(PublisherMessageVector& messages, const PublisherHeaders& header = PublisherHeaders()) {
    counter += messages.size();
    l.count_down(messages.size());
    return 0;
  }
  size_t
  getQueueMessageSize() {
    return counter;
  }
};

class DummyPublisherNoSignal : public IPublisher {
 public:
  std::vector<PublishMessageSharedPtr> sent_messages;
  DummyPublisherNoSignal() : IPublisher(std::make_unique<const PublisherConfiguration>(PublisherConfiguration{.server_address = "fake_address"})){};
  ~DummyPublisherNoSignal() = default;
  void
  setAutoPoll(bool autopoll) {}
  int
  setCallBackForReqType(const std::string req_type, EventCallback eventCallback) {
    return 0;
  }
  int
  createQueue(const std::string& queue) {
    return 0;
  }
  int
  flush(const int timeo) {
    return 0;
  }
  int
  pushMessage(PublishMessageUniquePtr message, const PublisherHeaders& header = PublisherHeaders()) {
    PublishMessageUniquePtr tmp_ptr_for_clean_data = std::move(message);
    return 0;
  }
  int
  pushMessages(PublisherMessageVector& messages, const PublisherHeaders& header = PublisherHeaders()) {
    messages.clear();
    return 0;
  }
  size_t
  getQueueMessageSize() {
    return sent_messages.size();
  }
};

#ifdef __linux__

std::unique_ptr<NodeController>
initBackend(IPublisherShrdPtr pub, bool clear_data = true, bool enable_debug_log = false) {
  int         argc    = 1;
  const char* argv[1] = {"epics-k2eg-test"};
  clearenv();
  if (enable_debug_log) {
    setenv("EPICS_k2eg_log-on-console", "true", 1);
    setenv("EPICS_k2eg_log-level", "debug", 1);
  } else {
    setenv("EPICS_k2eg_log-on-console", "false", 1);
  }
  setenv("EPICS_k2eg_metric-server-http-port", std::to_string(++tcp_port).c_str(), 1);
  std::unique_ptr<ProgramOptions> opt = std::make_unique<ProgramOptions>();
  opt->parse(argc, argv);
  ServiceResolver<ILogger>::registerService(std::make_shared<BoostLogger>(opt->getloggerConfiguration()));
  ServiceResolver<IMetricService>::registerService(std::make_shared<PrometheusMetricService>(opt->getMetricConfiguration()));
  ServiceResolver<EpicsServiceManager>::registerService(std::make_shared<EpicsServiceManager>());
  ServiceResolver<IPublisher>::registerService(pub);
  DataStorageUPtr storage = std::make_unique<DataStorage>(fs::path(fs::current_path()) / "test.sqlite");
  if (clear_data) { toShared(storage->getChannelRepository())->removeAll(); }
  return std::make_unique<NodeController>(std::move(storage));
}

void
deinitBackend(std::unique_ptr<NodeController> node_controller) {
  node_controller.reset();
  EXPECT_NO_THROW(ServiceResolver<IPublisher>::resolve().reset(););
  EXPECT_NO_THROW(ServiceResolver<EpicsServiceManager>::resolve().reset(););
  EXPECT_NO_THROW(ServiceResolver<IMetricService>::resolve().reset(););
  EXPECT_NO_THROW(ServiceResolver<ILogger>::resolve().reset(););
}

boost::json::object
getJsonObject(PublishMessage& published_message) {
  bj::error_code  ec;
  bj::string_view value_str = bj::string_view(published_message.getBufferPtr(), published_message.getBufferSize());
  auto            result    = bj::parse(value_str, ec).as_object();
  if (ec) throw std::runtime_error("invalid json");
  return result;
}
msgpack::unpacked
getMsgPackObject(PublishMessage& published_message) {
  msgpack::unpacked msg_upacked;
  msgpack::unpack(msg_upacked, published_message.getBufferPtr(), published_message.getBufferSize());
  return msg_upacked;
}

TEST(NodeController, MonitorCommandJsonSerByDefault) {
  std::latch                      work_done{1};
  std::unique_ptr<NodeController> node_controller;
  auto                            publisher = std::make_shared<DummyPublisher>(work_done);
  node_controller                           = initBackend(publisher);

  EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const MonitorCommand>(
      MonitorCommand{CommandType::monitor, SerializationType::JSON, "pva", "channel:ramp:ramp", true, KAFKA_TOPIC_ACQUIRE_IN})}););

  work_done.wait();
  // we need to have publish some message
  size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
  EXPECT_NE(published, 0);

  // stop acquire
  EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const MonitorCommand>(
      MonitorCommand{CommandType::monitor, SerializationType::JSON, "", "channel:ramp:ramp", false, KAFKA_TOPIC_ACQUIRE_IN})}););

  sleep(1);
  EXPECT_NO_THROW(published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize(););
  sleep(2);
  EXPECT_EQ(ServiceResolver<IPublisher>::resolve()->getQueueMessageSize(), published);

  // check that we have json data
  EXPECT_NO_THROW(auto json_object = getJsonObject(*publisher->sent_messages[0]););
  // dispose all
  deinitBackend(std::move(node_controller));
}

TEST(NodeController, MonitorCommandMsgPackSer) {
  std::latch                      work_done{1};
  std::unique_ptr<NodeController> node_controller;
  auto                            publisher = std::make_shared<DummyPublisher>(work_done);
  node_controller                           = initBackend(publisher);

  EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const MonitorCommand>(
      MonitorCommand{CommandType::monitor, SerializationType::Msgpack, "pva", "channel:ramp:ramp", true, KAFKA_TOPIC_ACQUIRE_IN})}););

  work_done.wait();
  // we need to have publish some message
  size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
  EXPECT_NE(published, 0);

  // stop acquire
  EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const MonitorCommand>(
      MonitorCommand{CommandType::monitor, SerializationType::Msgpack, "", "channel:ramp:ramp", false, KAFKA_TOPIC_ACQUIRE_IN})}););

  sleep(1);
  EXPECT_NO_THROW(published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize(););
  sleep(2);
  EXPECT_EQ(ServiceResolver<IPublisher>::resolve()->getQueueMessageSize(), published);

  // check that we have msgpack data
  msgpack::unpacked msgpack_unpacked;
  msgpack::object   msgpack_object;
  EXPECT_NO_THROW(msgpack_unpacked = getMsgPackObject(*publisher->sent_messages[0]););
  msgpack_object = msgpack_unpacked.get();
  EXPECT_EQ(msgpack_object.type, msgpack::type::MAP);
  // dispose all
  deinitBackend(std::move(node_controller));
}

TEST(NodeController, MonitorCommandMsgPackCompactSer) {
  std::latch                      work_done{1};
  std::unique_ptr<NodeController> node_controller;
  auto                            publisher = std::make_shared<DummyPublisher>(work_done);
  node_controller                           = initBackend(publisher);

  EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const MonitorCommand>(
      MonitorCommand{CommandType::monitor, SerializationType::MsgpackCompact, "pva", "channel:ramp:ramp", true, KAFKA_TOPIC_ACQUIRE_IN})}););

  work_done.wait();
  // we need to have publish some message
  size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
  EXPECT_NE(published, 0);

  // stop acquire
  EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const MonitorCommand>(
      MonitorCommand{CommandType::monitor, k2eg::common::SerializationType::MsgpackCompact, "", "channel:ramp:ramp", false, KAFKA_TOPIC_ACQUIRE_IN})}););

  sleep(1);
  EXPECT_NO_THROW(published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize(););
  sleep(2);
  EXPECT_EQ(ServiceResolver<IPublisher>::resolve()->getQueueMessageSize(), published);

  // check that we have msgpack compact
  msgpack::unpacked msgpack_unpacked;
  msgpack::object   msgpack_object;
  EXPECT_NO_THROW(msgpack_unpacked = getMsgPackObject(*publisher->sent_messages[0]););
  msgpack_object = msgpack_unpacked.get();
  EXPECT_EQ(msgpack_object.type, msgpack::type::ARRAY);
  // dispose all
  deinitBackend(std::move(node_controller));
}

TEST(NodeController, MonitorCommandAfterReboot) {
  std::latch work_done{1};
  std::latch work_done_2{1};

  auto node_controller = initBackend(std::make_shared<DummyPublisher>(work_done));

  EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const MonitorCommand>(
      MonitorCommand{CommandType::monitor, SerializationType::JSON, "pva", "channel:ramp:ramp", true, KAFKA_TOPIC_ACQUIRE_IN})}););

  work_done.wait();
  // we need to have publish some message
  size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
  EXPECT_NE(published, 0);

  // stop the node controller
  deinitBackend(std::move(node_controller));

  // reboot without delete database
  node_controller = initBackend(std::make_shared<DummyPublisher>(work_done_2), false);
  node_controller->reloadPersistentCommand();
  work_done_2.wait();
  // we need to have publish some message
  published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
  EXPECT_NE(published, 0);

  // dispose all
  deinitBackend(std::move(node_controller));
}

TEST(NodeController, GetCommandJson) {
  boost::json::object             json_object;
  std::latch                      work_done{1};
  std::shared_ptr<DummyPublisher> publisher = std::make_shared<DummyPublisher>(work_done);
  // set environment variable for test
  auto node_controller = initBackend(publisher);

  EXPECT_NO_THROW(node_controller->submitCommand(
      {std::make_shared<const GetCommand>(GetCommand{CommandType::get, SerializationType::JSON, "pva", "channel:ramp:ramp", KAFKA_TOPIC_ACQUIRE_IN})}););

  work_done.wait();
  // we need to have publish some message
  size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
  EXPECT_NE(published, 0);
  // check for json forward
  EXPECT_NO_THROW(json_object = getJsonObject(*publisher->sent_messages[0]););
  std::cout << json_object << std::endl;
  EXPECT_NE(json_object.if_contains("error"), nullptr);
  EXPECT_NE(json_object.if_contains(KEY_REPLY_ID), nullptr);
  EXPECT_NE(json_object.if_contains("channel:ramp:ramp"), nullptr);
  // dispose all
  deinitBackend(std::move(node_controller));
}

TEST(NodeController, GetCommandJsonWithReplyID) {
  boost::json::object             json_obj;
  std::latch                      work_done{1};
  std::shared_ptr<DummyPublisher> publisher = std::make_shared<DummyPublisher>(work_done);
  // set environment variable for test
  auto node_controller = initBackend(publisher);

  EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const GetCommand>(
      GetCommand{CommandType::get, SerializationType::JSON, "pva", "channel:ramp:ramp", KAFKA_TOPIC_ACQUIRE_IN, "REP_ID_JSON"})}););

  work_done.wait();
  // we need to have publish some message
  size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
  EXPECT_NE(published, 0);
  // check for json forward
  EXPECT_NO_THROW(json_obj = getJsonObject(*publisher->sent_messages[0]););
  EXPECT_EQ(json_obj.contains(KEY_REPLY_ID), true);
  EXPECT_STREQ(json_obj[KEY_REPLY_ID].as_string().c_str(), "REP_ID_JSON");
  // dispose all
  deinitBackend(std::move(node_controller));
}

TEST(NodeController, GetCommandMsgPack) {
  std::latch                      work_done{1};
  std::shared_ptr<DummyPublisher> publisher = std::make_shared<DummyPublisher>(work_done);
  // set environment variable for test
  auto node_controller = initBackend(publisher);

  EXPECT_NO_THROW(node_controller->submitCommand(
      {std::make_shared<const GetCommand>(GetCommand{CommandType::get, SerializationType::Msgpack, "pva", "channel:ramp:ramp", KAFKA_TOPIC_ACQUIRE_IN})}););

  work_done.wait();
  // we need to have publish some message
  size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
  EXPECT_NE(published, 0);
  // check for msgpack map
  msgpack::unpacked msgpack_unpacked;
  msgpack::object   msgpack_object;
  EXPECT_NO_THROW(msgpack_unpacked = getMsgPackObject(*publisher->sent_messages[0]););
  msgpack_object = msgpack_unpacked.get();
  EXPECT_EQ(msgpack_object.type, msgpack::type::MAP);
  // dispose all
  deinitBackend(std::move(node_controller));
}

TEST(NodeController, GetCommandMsgPackReplyID) {
  typedef std::map<std::string, msgpack::object> MapTest;
  std::latch                                     work_done{1};
  std::shared_ptr<DummyPublisher>                publisher = std::make_shared<DummyPublisher>(work_done);
  // set environment variable for test
  auto node_controller = initBackend(publisher);

  EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const GetCommand>(
      GetCommand{CommandType::get, SerializationType::Msgpack, "pva", "channel:ramp:ramp", KAFKA_TOPIC_ACQUIRE_IN, "REPLY_ID_MSGPACK"})}););

  work_done.wait();
  // we need to have publish some message
  size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
  EXPECT_NE(published, 0);
  // check for msgpack map
  msgpack::unpacked msgpack_unpacked;
  msgpack::object   msgpack_object;
  EXPECT_NO_THROW(msgpack_unpacked = getMsgPackObject(*publisher->sent_messages[0]););
  msgpack_object = msgpack_unpacked.get();
  auto mt        = msgpack_object.as<MapTest>();
  EXPECT_EQ(mt.contains(KEY_REPLY_ID), true);
  EXPECT_STREQ(mt[KEY_REPLY_ID].as<std::string>().c_str(), "REPLY_ID_MSGPACK");
  EXPECT_EQ(msgpack_object.type, msgpack::type::MAP);
  // dispose all
  deinitBackend(std::move(node_controller));
}

TEST(NodeController, GetCommandMsgPackCompack) {
  std::latch                      work_done{1};
  std::shared_ptr<DummyPublisher> publisher = std::make_shared<DummyPublisher>(work_done);
  // set environment variable for test
  auto node_controller = initBackend(publisher);

  EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const GetCommand>(
      GetCommand{CommandType::get, SerializationType::MsgpackCompact, "pva", "channel:ramp:ramp", KAFKA_TOPIC_ACQUIRE_IN})}););

  work_done.wait();
  // we need to have publish some message
  size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
  EXPECT_NE(published, 0);
  // check for masgpack compact array
  msgpack::unpacked msgpack_unpacked;
  msgpack::object   msgpack_object;
  EXPECT_NO_THROW(msgpack_unpacked = getMsgPackObject(*publisher->sent_messages[0]););
  EXPECT_NO_THROW(msgpack_object = msgpack_unpacked.get(););
  EXPECT_EQ(msgpack_object.type, msgpack::type::ARRAY);
  // dispose all
  deinitBackend(std::move(node_controller));
}

TEST(NodeController, GetCommandMsgPackCompackWithReplyID) {
  typedef std::vector<msgpack::object> VecTest;
  std::latch                           work_done{1};
  std::shared_ptr<DummyPublisher>      publisher = std::make_shared<DummyPublisher>(work_done);
  // set environment variable for test
  auto node_controller = initBackend(publisher);

  EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const GetCommand>(
      GetCommand{CommandType::get, SerializationType::MsgpackCompact, "pva", "channel:ramp:ramp", KAFKA_TOPIC_ACQUIRE_IN, "REPLY_ID_MSGPACK_COMPACT"})}););

  work_done.wait();
  // we need to have publish some message
  size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
  EXPECT_NE(published, 0);
  // check for masgpack compact array
  msgpack::unpacked msgpack_unpacked;
  msgpack::object   msgpack_object;
  EXPECT_NO_THROW(msgpack_unpacked = getMsgPackObject(*publisher->sent_messages[0]););
  msgpack_object = msgpack_unpacked.get();
  EXPECT_EQ(msgpack_object.type, msgpack::type::ARRAY);
  auto res_vec = msgpack_object.as<VecTest>();
  EXPECT_EQ(res_vec[0].type, msgpack::type::STR);
  EXPECT_STREQ(res_vec[0].as<std::string>().c_str(), "REPLY_ID_MSGPACK_COMPACT");
  // dispose all
  deinitBackend(std::move(node_controller));
}

inline void
wait_latch(std::latch& l) {
  bool w       = true;
  int  counter = 10;
  while (w && counter > 0) {
    if (l.try_wait()) {
      w = false;
    } else {
      counter--;
      std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
  }
}

TEST(NodeController, GetCommandCAChannel) {
  auto publisher = std::make_shared<DummyPublisherCounter>(1);
  // set environment variable for test
  auto node_controller = initBackend(publisher);

  EXPECT_NO_THROW(node_controller->submitCommand(
      {std::make_shared<const GetCommand>(GetCommand{CommandType::get, SerializationType::JSON, "ca", "variable:sum", KAFKA_TOPIC_ACQUIRE_IN})}););
  // give some time for the timeout
  wait_latch(publisher->l);
  // we need to have publish some message
  size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
  EXPECT_NE(published, 0);

  // dispose all
  deinitBackend(std::move(node_controller));
}

TEST(NodeController, GetCommandBadChannel) {
  std::latch work_done{1};
  // set environment variable for test
  auto node_controller = initBackend(std::make_shared<DummyPublisher>(work_done));

  EXPECT_NO_THROW(node_controller->submitCommand(
      {std::make_shared<const GetCommand>(GetCommand{CommandType::get, SerializationType::JSON, "pva", "bad:channel:name", KAFKA_TOPIC_ACQUIRE_IN})}););
  // give some time for the timeout
  sleep(5);
  // we need to have publish some message
  size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
  EXPECT_EQ(published, 0);

  // dispose all
  deinitBackend(std::move(node_controller));
}

TEST(NodeController, PutCommandBadChannel) {
  std::latch work_done{1};
  // set environment variable for test
  auto node_controller = initBackend(std::make_shared<DummyPublisher>(work_done));

  EXPECT_NO_THROW(node_controller->submitCommand(
      {std::make_shared<const PutCommand>(PutCommand{CommandType::put, SerializationType::Unknown, "pva", "bad:channel:name", "1"})}););

  // this should give the timeout of the put command so the node controller will exit without problem

  // dispose all
  deinitBackend(std::move(node_controller));
}

typedef std::vector<msgpack::object> MsgpackObjectVector;
TEST(NodeController, PutCommandScalar) {
  std::random_device                 r;
  std::default_random_engine         e1(r());
  std::uniform_int_distribution<int> uniform_dist(1, 100);
  std::latch                         work_done{1};
  ConstChannelDataUPtr               value_readout;
  std::shared_ptr<DummyPublisher>    publisher = std::make_shared<DummyPublisher>(work_done);
  // set environment variable for test
  auto node_controller = initBackend(publisher);
  auto random_scalar   = uniform_dist(e1);
  EXPECT_NO_THROW(node_controller->submitCommand(
      {std::make_shared<const PutCommand>(PutCommand{CommandType::put, SerializationType::Unknown, "pva", "variable:b", std::to_string(random_scalar)})}););
  // give some time for the timeout
  // sleep(2);

  EXPECT_NO_THROW(node_controller->submitCommand(
      {std::make_shared<const GetCommand>(GetCommand{CommandType::get, SerializationType::MsgpackCompact, "pva", "variable:b", KAFKA_TOPIC_ACQUIRE_IN})}););

  // wait for the result of get command
  work_done.wait();
  msgpack::unpacked msgpack_unpacked;
  msgpack::object   msgpack_object;
  EXPECT_NO_THROW(msgpack_unpacked = getMsgPackObject(*publisher->sent_messages[0]););
  msgpack_object = msgpack_unpacked.get();
  EXPECT_EQ(msgpack_object.type, msgpack::type::ARRAY);

  auto vec = msgpack_object.as<MsgpackObjectVector>();
  EXPECT_EQ(vec[1].type, msgpack::type::POSITIVE_INTEGER);
  EXPECT_EQ(vec[1].as<int>(), random_scalar);
  // dispose all
  deinitBackend(std::move(node_controller));
}

TEST(NodeController, PutCommandScalarArray) {
  std::latch                      work_done{1};
  ConstChannelDataUPtr            value_readout;
  std::shared_ptr<DummyPublisher> publisher = std::make_shared<DummyPublisher>(work_done);
  // set environment variable for test
  auto node_controller = initBackend(publisher);

  EXPECT_NO_THROW(node_controller->submitCommand(
      {std::make_shared<const PutCommand>(PutCommand{CommandType::put, SerializationType::Unknown, "pva", "channel:waveform", "8 0 0 0 0 0 0 0 0"})}););
  // give some time for the timeout
  // sleep(2);

  EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const GetCommand>(
      GetCommand{CommandType::get, SerializationType::MsgpackCompact, "pva", "channel:waveform", KAFKA_TOPIC_ACQUIRE_IN})}););

  // wait for the result of get command
  work_done.wait();
  msgpack::unpacked msgpack_unpacked;
  msgpack::object   msgpack_object;
  EXPECT_NO_THROW(msgpack_unpacked = getMsgPackObject(*publisher->sent_messages[0]););
  msgpack_object = msgpack_unpacked.get();
  EXPECT_EQ(msgpack_object.type, msgpack::type::ARRAY);

  auto vec = msgpack_object.as<MsgpackObjectVector>();
  EXPECT_EQ(vec[1].type, msgpack::type::ARRAY);

  auto value_vec = vec[1].as<MsgpackObjectVector>();
  EXPECT_EQ(value_vec[0].type, msgpack::type::POSITIVE_INTEGER);
  ;

  // dispose all
  deinitBackend(std::move(node_controller));
}

#endif  // __linux__
TEST(NodeController, RandomCommand) {
  std::random_device                 r;
  std::default_random_engine         e1(r());
  std::uniform_int_distribution<int> uniform_dist(0, 3);
  std::uniform_int_distribution<int> uniform_dist_sleep(500, 1000);
  // set environment variable for test
  auto node_controller = initBackend(std::make_shared<DummyPublisherNoSignal>());

  // send 100 random commands equence iteration
  for (int idx = 0; idx < 100; idx++) {
    int rand_selection = uniform_dist(e1);
    switch (rand_selection) {
      case 0: {
        std::this_thread::sleep_for(std::chrono::milliseconds(uniform_dist_sleep(e1)));
        EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const MonitorCommand>(
            MonitorCommand{CommandType::monitor, SerializationType::JSON, "pva", "channel:ramp:ramp", true, KAFKA_TOPIC_ACQUIRE_IN})}););
        break;
      }
      case 1: {
        std::this_thread::sleep_for(std::chrono::milliseconds(uniform_dist_sleep(e1)));
        EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const MonitorCommand>(
            MonitorCommand{CommandType::monitor, SerializationType::JSON, "pva", "channel:ramp:ramp", false, KAFKA_TOPIC_ACQUIRE_IN})}););
        break;
      }
      case 2: {
        std::this_thread::sleep_for(std::chrono::milliseconds(uniform_dist_sleep(e1)));
        EXPECT_NO_THROW(node_controller->submitCommand(
            {std::make_shared<const GetCommand>(GetCommand{CommandType::get, SerializationType::JSON, "pva", "variable:b", KAFKA_TOPIC_ACQUIRE_IN})}););
        break;
      }

      case 3: {
        auto random_scalar = uniform_dist(e1);
        std::this_thread::sleep_for(std::chrono::milliseconds(uniform_dist_sleep(e1)));
        EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const PutCommand>(
            PutCommand{CommandType::put, SerializationType::Unknown, "pva", "variable:b", std::to_string(random_scalar)})}););
      }
    }
  }
  node_controller->waitForTaskCompletion();
  // dispose all
  deinitBackend(std::move(node_controller));
}