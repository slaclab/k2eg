#include <gtest/gtest.h>
#include <k2eg/common/ProgramOptions.h>
#include <k2eg/common/uuid.h>
#include <k2eg/controller/command/CMDController.h>
#include <k2eg/service/ServiceResolver.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/log/impl/BoostLogger.h>
#include <k2eg/service/pubsub/pubsub.h>
#include <k2eg/service/metric/IEpicsMetric.h>
#include <k2eg/service/metric/impl/DummyMetricService.h>

#include <algorithm>
#include <boost/json.hpp>
#include <climits>
#include <filesystem>
#include <functional>
#include <random>
#include <tuple>
#include <vector>
#include "k2eg/controller/command/cmd/Command.h"
#include "k2eg/controller/command/cmd/MonitorCommand.h"
#include "k2eg/controller/command/cmd/SnapshotCommand.h"
#include "k2eg/service/pubsub/IPublisher.h"
#include "k2eg/service/pubsub/impl/kafka/RDKafkaPublisher.h"

using namespace k2eg::common;

using namespace k2eg::controller::command;
using namespace k2eg::controller::command::cmd;

using namespace k2eg::service;
using namespace k2eg::service::log;
using namespace k2eg::service::log::impl;
using namespace k2eg::service::pubsub;
using namespace k2eg::service::pubsub::impl::kafka;
using namespace k2eg::service::metric;
using namespace k2eg::service::metric::impl;
using namespace boost::json;

#define KAFKA_ADDR "kafka:9092"
#define CMD_QUEUE  "cmd_topic_in"

class CMDMessage : public PublishMessage {
  const std::string request_type;
  const std::string distribution_key;
  const std::string queue;
  //! the message data
  const std::string message;

 public:
  CMDMessage(const std::string& queue, const std::string& message)
      : request_type("command"), distribution_key(UUID::generateUUIDLite()), queue(queue), message(message) {}
  virtual ~CMDMessage() = default;

  char*
  getBufferPtr() {
    return const_cast<char*>(message.c_str());
  }
  const size_t
  getBufferSize() {
    return message.size();
  }
  const std::string&
  getQueue() {
    return queue;
  }
  const std::string&
  getDistributionKey() {
    return distribution_key;
  }
  const std::string&
  getReqType() {
    return request_type;
  }
};
#ifdef __linux__
TEST(CMDController, CheckConfiguration) {
  int                         argc    = 1;
  const char*                 argv[1] = {"epics-k2eg-test"};
  CMDControllerCommandHandler handler = [](ConstCommandShrdPtrVec received_command) {};
  // set environment variable for test
  clearenv();
  setenv("EPICS_k2eg_log-on-console", "false", 1);
  setenv("EPICS_k2eg_sub-server-address", KAFKA_ADDR, 1);
  setenv("EPICS_k2eg_cmd-input-topic", CMD_QUEUE, 1);
  std::unique_ptr<ProgramOptions> opt = std::make_unique<ProgramOptions>();
  ASSERT_NO_THROW(opt->parse(argc, argv));
  ServiceResolver<IMetricService>::registerService(std::make_shared<DummyMetricService>(opt->getMetricConfiguration()));
  ServiceResolver<ILogger>::registerService(std::make_shared<BoostLogger>(opt->getloggerConfiguration()));
  ServiceResolver<ISubscriber>::registerService(std::make_shared<RDKafkaSubscriber>(opt->getSubscriberConfiguration()));
  std::unique_ptr<CMDController> cmd_controller = std::make_unique<CMDController>(opt->getCMDControllerConfiguration(), handler);
  EXPECT_STREQ(cmd_controller->configuration->topic_in.c_str(), CMD_QUEUE);
  ASSERT_NO_THROW(cmd_controller.reset(););
  ServiceResolver<ISubscriber>::resolve().reset();
  ServiceResolver<ILogger>::resolve().reset();
}

TEST(CMDController, InitFaultCheckWithNoQueue) {
  int                         argc    = 1;
  const char*                 argv[1] = {"epics-k2eg-test"};
  CMDControllerCommandHandler handler = [](ConstCommandShrdPtrVec received_command) {};
  // set environment variable for test
  clearenv();
  setenv("EPICS_k2eg_log-on-console", "false", 1);
  setenv("EPICS_k2eg_sub-server-address", KAFKA_ADDR, 1);
  std::unique_ptr<ProgramOptions> opt = std::make_unique<ProgramOptions>();
  ASSERT_NO_THROW(opt->parse(argc, argv));
  ServiceResolver<IMetricService>::registerService(std::make_shared<DummyMetricService>(opt->getMetricConfiguration()));
  ServiceResolver<ILogger>::registerService(std::make_shared<BoostLogger>(opt->getloggerConfiguration()));
  ServiceResolver<ISubscriber>::registerService(std::make_shared<RDKafkaSubscriber>(opt->getSubscriberConfiguration()));
  ASSERT_ANY_THROW(std::make_unique<CMDController>(opt->getCMDControllerConfiguration(), handler););
  ServiceResolver<ISubscriber>::resolve().reset();
  ServiceResolver<ILogger>::resolve().reset();
}

TEST(CMDController, StartStop) {
  int                         argc    = 1;
  const char*                 argv[1] = {"epics-k2eg-test"};
  CMDControllerCommandHandler handler = [](ConstCommandShrdPtrVec received_command) {};
  // set environment variable for test
  clearenv();
  setenv("EPICS_k2eg_log-on-console", "false", 1);
  setenv("EPICS_k2eg_cmd-input-topic", CMD_QUEUE, 1);
  setenv("EPICS_k2eg_sub-server-address", KAFKA_ADDR, 1);
  std::unique_ptr<ProgramOptions> opt = std::make_unique<ProgramOptions>();
  ASSERT_NO_THROW(opt->parse(argc, argv));
  ServiceResolver<IMetricService>::registerService(std::make_shared<DummyMetricService>(opt->getMetricConfiguration()));
  ServiceResolver<ILogger>::registerService(std::make_shared<BoostLogger>(opt->getloggerConfiguration()));
  ServiceResolver<ISubscriber>::registerService(std::make_shared<RDKafkaSubscriber>(opt->getSubscriberConfiguration()));
  std::unique_ptr<CMDController> cmd_controller = std::make_unique<CMDController>(opt->getCMDControllerConfiguration(), handler);
  ASSERT_NO_THROW(cmd_controller.reset(););
  ServiceResolver<ISubscriber>::resolve().reset();
  ServiceResolver<ILogger>::resolve().reset();
}

class CMDControllerCommandTestParametrized : public ::testing::TestWithParam<std::tuple<CMDControllerCommandHandler, std::string>> {
  std::unique_ptr<CMDController>         cmd_controller;
  static std::unique_ptr<ProgramOptions> opt;

 public:
  static void
  SetUpTestCase() {
    sleep(5);
    clearenv();
    int         argc    = 1;
    const char* argv[1] = {"epics-k2eg-test"};
    setenv("EPICS_k2eg_log-on-console", "false", 1);
    setenv("EPICS_k2eg_cmd-input-topic", CMD_QUEUE, 1);
    setenv("EPICS_k2eg_cmd-max-fecth-element", "100", 1);
    setenv("EPICS_k2eg_cmd-max-fecth-time-out", "100", 1);
    setenv("EPICS_k2eg_sub-server-address", KAFKA_ADDR, 1);
    setenv("EPICS_k2eg_pub-server-address", KAFKA_ADDR, 1);
    setenv("EPICS_k2eg_sub-group-id", "", 1);
    opt = std::make_unique<ProgramOptions>();
    opt->parse(argc, argv);
    ServiceResolver<IMetricService>::registerService(std::make_shared<DummyMetricService>(opt->getMetricConfiguration()));
    ServiceResolver<ILogger>::registerService(std::make_shared<BoostLogger>(opt->getloggerConfiguration()));
    ServiceResolver<ISubscriber>::registerService(std::make_shared<RDKafkaSubscriber>(opt->getSubscriberConfiguration()));
    ServiceResolver<IPublisher>::registerService(std::make_shared<RDKafkaPublisher>(opt->getPublisherConfiguration()));
  }

  static void
  TearDownTestCase() {
    ServiceResolver<ISubscriber>::resolve().reset();
    ServiceResolver<ILogger>::resolve().reset();
  }

  void
  SetUp() {
    sleep(5);
    CMDControllerCommandHandler handler = std::get<0>(GetParam());
    ASSERT_NO_THROW(cmd_controller = std::make_unique<CMDController>(opt->getCMDControllerConfiguration(), handler););
  }

  void
  TearDown() {
    cmd_controller.reset();
  }
};
std::unique_ptr<ProgramOptions> CMDControllerCommandTestParametrized::opt;

TEST_P(CMDControllerCommandTestParametrized, CheckCommand) {
  std::string message = std::get<1>(GetParam());
  // start producer for send command
  std::unique_ptr<IPublisher> publisher =
      std::make_unique<RDKafkaPublisher>(std::make_unique<const PublisherConfiguration>(PublisherConfiguration{.server_address = KAFKA_ADDR}));
  std::this_thread::sleep_for(std::chrono::seconds(2));
  publisher->pushMessage(std::make_unique<CMDMessage>(CMD_QUEUE, message));
  publisher->flush(100);
  publisher.reset();
}

//------------------------------ command tests -------------------------
CMDControllerCommandHandler acquire_test_default_ser = [](ConstCommandShrdPtrVec received_command) {
  ASSERT_EQ(received_command.size(), 1);
  ASSERT_EQ(received_command[0]->type, CommandType::monitor);
  ASSERT_EQ(received_command[0]->serialization, SerializationType::Msgpack); // the default serializaiton value
  // ASSERT_EQ(received_command[0]->protocol.compare("pva"), 0);
  ASSERT_EQ(reinterpret_cast<const MonitorCommand*>(received_command[0].get())->pv_name.compare("pva://channel::a"), 0);
  ASSERT_EQ(reinterpret_cast<const MonitorCommand*>(received_command[0].get())->reply_topic.compare("topic-dest"), 0);
  ASSERT_EQ(reinterpret_cast<const MonitorCommand*>(received_command[0].get())->monitor_destination_topic.compare("topic-dest"), 0);
};
boost::json::value acquire_default_ser = {
    {KEY_COMMAND, "monitor"}, {KEY_PV_NAME, "pva://channel::a"}, {KEY_REPLY_TOPIC, "topic-dest"}};

CMDControllerCommandHandler acquire_multi_pv_test_default_ser = [](ConstCommandShrdPtrVec received_command) {
  ASSERT_EQ(received_command.size(), 1);
  ASSERT_EQ(received_command[0]->type, CommandType::multi_monitor);
  ASSERT_EQ(received_command[0]->serialization, SerializationType::Msgpack); // the default serializaiton value
  // ASSERT_EQ(received_command[0]->protocol.compare("pva"), 0);
  const std::vector<std::string>& pv_array = reinterpret_cast<const MultiMonitorCommand*>(received_command[0].get())->pv_name_list;
  ASSERT_NE(std::find(pv_array.begin(), pv_array.end(), "pva://channel::a"), pv_array.end());
  ASSERT_NE(std::find(pv_array.begin(), pv_array.end(), "pva://channel::b"), pv_array.end());
  ASSERT_EQ(reinterpret_cast<const MultiMonitorCommand*>(received_command[0].get())->reply_topic.compare("topic-dest"), 0);
};
boost::json::value acquire_multi_pv_default_ser = {
    {KEY_COMMAND, "monitor"}, {KEY_PV_NAME, {"pva://channel::a", "pva://channel::b"}}, {KEY_REPLY_TOPIC, "topic-dest"}};


CMDControllerCommandHandler acquire_test_specific_monitor_dest_topic = [](ConstCommandShrdPtrVec received_command) {
  ASSERT_EQ(received_command.size(), 1);
  ASSERT_EQ(received_command[0]->type, CommandType::monitor);
  ASSERT_EQ(received_command[0]->serialization, SerializationType::Msgpack); // the default serializaiton value
  // ASSERT_EQ(received_command[0]->protocol.compare("pva"), 0);
  ASSERT_EQ(reinterpret_cast<const MonitorCommand*>(received_command[0].get())->pv_name.compare("pva://channel::a"), 0);
  ASSERT_EQ(reinterpret_cast<const MonitorCommand*>(received_command[0].get())->reply_topic.compare("topic-dest"), 0);
  ASSERT_EQ(reinterpret_cast<const MonitorCommand*>(received_command[0].get())->monitor_destination_topic.compare("monitor-topic-dest"), 0);
};
boost::json::value acquire_specific_monitor_dest_topic = {
    {KEY_COMMAND, "monitor"}, {KEY_PV_NAME, "pva://channel::a"}, {KEY_REPLY_TOPIC, "topic-dest"}, {KEY_MONITOR_DEST_TOPIC, "monitor-topic-dest"}};

CMDControllerCommandHandler acquire_test_json = [](ConstCommandShrdPtrVec received_command) {
  ASSERT_EQ(received_command.size(), 1);
  ASSERT_EQ(received_command[0]->type, CommandType::monitor);
  ASSERT_EQ(received_command[0]->serialization, SerializationType::JSON);
  ASSERT_EQ(reinterpret_cast<const MonitorCommand*>(received_command[0].get())->pv_name.compare("pva://channel::a"), 0);
  ASSERT_EQ(reinterpret_cast<const MonitorCommand*>(received_command[0].get())->reply_topic.compare("topic-dest"), 0);
  ASSERT_EQ(reinterpret_cast<const MonitorCommand*>(received_command[0].get())->monitor_destination_topic.compare("topic-dest"), 0);
};
boost::json::value acquire_json = {{KEY_COMMAND, "monitor"},
                                   {KEY_SERIALIZATION, "json"},
                                  //  {KEY_PROTOCOL, "pva"},
                                   {KEY_PV_NAME, "pva://channel::a"},
                                   {KEY_REPLY_TOPIC, "topic-dest"}};

CMDControllerCommandHandler acquire_test_msgpack = [](ConstCommandShrdPtrVec received_command) {
  ASSERT_EQ(received_command.size(), 1);
  ASSERT_EQ(received_command[0]->type, CommandType::monitor);
  ASSERT_EQ(received_command[0]->serialization, SerializationType::Msgpack);
  // ASSERT_EQ(received_command[0]->protocol.compare("pva"), 0);
  ASSERT_EQ(reinterpret_cast<const MonitorCommand*>(received_command[0].get())->pv_name.compare("pva://channel::a"), 0);
  ASSERT_EQ(reinterpret_cast<const MonitorCommand*>(received_command[0].get())->reply_topic.compare("topic-dest"), 0);
  ASSERT_EQ(reinterpret_cast<const MonitorCommand*>(received_command[0].get())->monitor_destination_topic.compare("topic-dest"), 0);
};
boost::json::value acquire_msgpack = {{KEY_COMMAND, "monitor"},
                                      {KEY_SERIALIZATION, "msgpack"},
                                      // {KEY_PROTOCOL, "pva"},
                                      {KEY_PV_NAME, "pva://channel::a"},
                                      {KEY_REPLY_TOPIC, "topic-dest"}};

CMDControllerCommandHandler get_test_json = [](ConstCommandShrdPtrVec received_command) {
  ASSERT_EQ(received_command.size(), 1);
  ASSERT_EQ(received_command[0]->type, CommandType::get);
  ASSERT_EQ(received_command[0]->serialization, SerializationType::JSON);
  // ASSERT_EQ(received_command[0]->protocol.compare("pva"), 0);
  ASSERT_EQ(reinterpret_cast<const GetCommand*>(received_command[0].get())->pv_name.compare("pva://channel::a"), 0);
  ASSERT_EQ(reinterpret_cast<const GetCommand*>(received_command[0].get())->reply_topic.compare("topic-dest"), 0);
};
boost::json::value get_json = {
    {KEY_COMMAND, "get"}, {KEY_PV_NAME, "pva://channel::a"}, {KEY_SERIALIZATION, "json"}, {KEY_REPLY_TOPIC, "topic-dest"}};

CMDControllerCommandHandler get_test_msgpack = [](ConstCommandShrdPtrVec received_command) {
  ASSERT_EQ(received_command.size(), 1);
  ASSERT_EQ(received_command[0]->type, CommandType::get);
  ASSERT_EQ(received_command[0]->serialization, SerializationType::Msgpack);
  // ASSERT_EQ(received_command[0]->protocol.compare("pva"), 0);
  ASSERT_EQ(reinterpret_cast<const GetCommand*>(received_command[0].get())->pv_name.compare("pva://channel::a"), 0);
  ASSERT_EQ(reinterpret_cast<const GetCommand*>(received_command[0].get())->reply_topic.compare("topic-dest"), 0);
};
boost::json::value get_msgpack = {
    {KEY_COMMAND, "get"}, {KEY_PV_NAME, "pva://channel::a"}, {KEY_SERIALIZATION, "msgpack"}, {KEY_REPLY_TOPIC, "topic-dest"}};

CMDControllerCommandHandler put_test = [](ConstCommandShrdPtrVec received_command) {
  ASSERT_EQ(received_command.size(), 1);
  ASSERT_EQ(received_command[0]->type, CommandType::put);
  // ASSERT_EQ(received_command[0]->protocol.compare("pva"), 0);
  ASSERT_EQ(reinterpret_cast<const PutCommand*>(received_command[0].get())->pv_name.compare("pva://channel::a"), 0);
  ASSERT_EQ(reinterpret_cast<const PutCommand*>(received_command[0].get())->value.compare("set-value"), 0);
};
boost::json::value put_json = {{KEY_COMMAND, "put"}, {KEY_PV_NAME, "pva://channel::a"}, {KEY_VALUE, "set-value"}};

CMDControllerCommandHandler info_test = [](ConstCommandShrdPtrVec received_command) {
  ASSERT_EQ(received_command.size(), 1);
  ASSERT_EQ(received_command[0]->type, CommandType::info);
  // ASSERT_EQ(received_command[0]->protocol.compare("pva"), 0);
  ASSERT_EQ(reinterpret_cast<const GetCommand*>(received_command[0].get())->pv_name.compare("pva://channel::a"), 0);
  ASSERT_EQ(reinterpret_cast<const GetCommand*>(received_command[0].get())->reply_topic.compare("topic-dest"), 0);
};

boost::json::value info_json = {{KEY_COMMAND, "info"},{KEY_PV_NAME, "pva://channel::a"}, {KEY_REPLY_TOPIC, "topic-dest"}};

CMDControllerCommandHandler snapshot_test = [](ConstCommandShrdPtrVec received_command) {
  ASSERT_EQ(received_command.size(), 1);
  ASSERT_EQ(received_command[0]->type, CommandType::snapshot);
  // ASSERT_EQ(received_command[0]->protocol.compare("pva"), 0);
  auto pv_name_list = reinterpret_cast<const SnapshotCommand*>(received_command[0].get())->pv_name_list;
  ASSERT_TRUE(std::find(pv_name_list.begin(), pv_name_list.end(), "pva://channel::a") != pv_name_list.end());
  ASSERT_TRUE(std::find(pv_name_list.begin(), pv_name_list.end(), "pva://channel::b") != pv_name_list.end());
  ASSERT_EQ(reinterpret_cast<const SnapshotCommand*>(received_command[0].get())->reply_topic.compare("topic-dest"), 0);
};


boost::json::value snapshot_json = {
  {KEY_COMMAND, "snapshot"}, 
  {KEY_PV_NAME_LIST, {"pva://channel::a", "pva://channel::b"}}, 
  {KEY_REPLY_TOPIC, "topic-dest"}, 
  {KEY_REPLY_ID, "rep-id"}
};

CMDControllerCommandHandler recurring_snapshot_test = [](ConstCommandShrdPtrVec received_command) {
  ASSERT_EQ(received_command.size(), 1);
  ASSERT_EQ(received_command[0]->type, CommandType::repeating_snapshot);
  // ASSERT_EQ(received_command[0]->protocol.compare("pva"), 0);
  auto pv_name_list = reinterpret_cast<const RepeatingSnapshotCommand*>(received_command[0].get())->pv_name_list;
  ASSERT_TRUE(std::find(pv_name_list.begin(), pv_name_list.end(), "pva://channel::a") != pv_name_list.end());
  ASSERT_TRUE(std::find(pv_name_list.begin(), pv_name_list.end(), "pva://channel::b") != pv_name_list.end());
  ASSERT_EQ(reinterpret_cast<const RepeatingSnapshotCommand*>(received_command[0].get())->reply_topic.compare("topic-dest"), 0);
  ASSERT_EQ(reinterpret_cast<const RepeatingSnapshotCommand*>(received_command[0].get())->reply_id.compare("rep-id"), 0);
  ASSERT_EQ(reinterpret_cast<const RepeatingSnapshotCommand*>(received_command[0].get())->snapshot_name.compare("snapshot-name"), 0);
  ASSERT_EQ(reinterpret_cast<const RepeatingSnapshotCommand*>(received_command[0].get())->repeat_delay_msec, 1000);
  ASSERT_EQ(reinterpret_cast<const RepeatingSnapshotCommand*>(received_command[0].get())->time_window_msec, 1000);
  ASSERT_EQ(reinterpret_cast<const RepeatingSnapshotCommand*>(received_command[0].get())->triggered, false);
  auto pv_field_filter_list = reinterpret_cast<const RepeatingSnapshotCommand*>(received_command[0].get())->pv_field_filter_list;
  ASSERT_TRUE(std::find(pv_field_filter_list.begin(), pv_field_filter_list.end(), "value") != pv_field_filter_list.end());
};

boost::json::value recurring_snapshot_json = {
  {KEY_COMMAND, "repeating_snapshot"},
  {KEY_REPLY_TOPIC, "topic-dest"},
  {KEY_REPLY_ID, "rep-id"},
  {KEY_SNAPSHOT_NAME, "snapshot-name"},
  {KEY_PV_NAME_LIST,{"pva://channel::a", "pva://channel::b"}},
  {KEY_TIME_WINDOW_MSEC, 1000},
  {KEY_REPEAT_DELAY_MSEC, 1000},
  {KEY_TRIGGERED, false},
  {KEY_PV_FIELD_FILTER_LIST, {"value"}}
};

CMDControllerCommandHandler recurring_snapshot_trigger_test = [](ConstCommandShrdPtrVec received_command) {
  ASSERT_EQ(received_command.size(), 1);
  ASSERT_EQ(received_command[0]->type, CommandType::repeating_snapshot_trigger);
  ASSERT_EQ(reinterpret_cast<const RepeatingSnapshotTriggerCommand*>(received_command[0].get())->snapshot_name.compare("snapshot-name"), 0);
};

boost::json::value recurring_snapshot_trigger_json = {
  {KEY_COMMAND, "repeating_snapshot_trigger"},
  {KEY_REPLY_TOPIC, "topic-dest"},
  {KEY_REPLY_ID, "rep-id"},
  {KEY_SNAPSHOT_NAME, "snapshot-name"},
};

CMDControllerCommandHandler recurring_snapshot_stop_test = [](ConstCommandShrdPtrVec received_command) {
  ASSERT_EQ(received_command.size(), 1);
  ASSERT_EQ(received_command[0]->type, CommandType::repeating_snapshot_stop);
  ASSERT_EQ(reinterpret_cast<const RepeatingSnapshotStopCommand*>(received_command[0].get())->snapshot_name.compare("snapshot-name"), 0);
};

boost::json::value recurring_snapshot_stop_json = {
  {KEY_COMMAND, "repeating_snapshot_stop"},
  {KEY_REPLY_TOPIC, "topic-dest"},
  {KEY_REPLY_ID, "rep-id"},
  {KEY_SNAPSHOT_NAME, "snapshot-name"}
};

boost::json::value bad_acquire_command = {{KEY_COMMAND, "monitor"}, {"destination", "topic-dest"}};

CMDControllerCommandHandler dummy_receiver = [](ConstCommandShrdPtrVec received_command) {};

using random_bytes_engine = std::independent_bits_engine<std::default_random_engine, CHAR_BIT, unsigned char>;

std::string
random_string(int size) {
  random_bytes_engine        rbe;
  std::vector<unsigned char> data(size);
  std::generate(begin(data), end(data), std::ref(rbe));
  return std::string(reinterpret_cast<const char*>(&data[0]), data.size());
}
boost::json::value       non_compliant_command_1 = {{KEY_COMMAND, "strange"}, {KEY_REPLY_TOPIC, "topic-dest"}};
boost::json::value       non_compliant_command_2 = {{"key", "value"}};
static const std::string bad_command_str         = "this is only a string";
static const std::string random_str1             = random_string(16);
static const std::string random_str2             = random_string(32);
static const std::string random_str3             = random_string(64);
static const std::string random_str4             = random_string(128);
static const std::string random_str5             = random_string(256);
static const std::string random_str6             = random_string(512);
static const std::string random_str7             = random_string(1024);
INSTANTIATE_TEST_CASE_P(CMDControllerCommandTest,
                        CMDControllerCommandTestParametrized,
                        ::testing::Values(std::make_tuple(acquire_test_default_ser, serialize(acquire_default_ser)),
                                          std::make_tuple(acquire_multi_pv_test_default_ser, serialize(acquire_multi_pv_default_ser)),
                                          std::make_tuple(acquire_test_specific_monitor_dest_topic, serialize(acquire_specific_monitor_dest_topic)),
                                          std::make_tuple(acquire_test_json, serialize(acquire_json)),
                                          std::make_tuple(acquire_test_msgpack, serialize(acquire_msgpack)),
                                          // std::make_tuple(acquire_test_msgpack_compact, serialize(acquire_msgpack_compact)),
                                          std::make_tuple(get_test_json, serialize(get_json)),
                                          std::make_tuple(get_test_msgpack, serialize(get_msgpack)),
                                          // std::make_tuple(get_test_msgpack_compact, serialize(get_msgpack_compact)),
                                          std::make_tuple(put_test, serialize(put_json)),
                                          std::make_tuple(info_test, serialize(info_json)),
                                          std::make_tuple(snapshot_test, serialize(snapshot_json)),
                                          std::make_tuple(recurring_snapshot_test, serialize(recurring_snapshot_json)),
                                          std::make_tuple(recurring_snapshot_trigger_test, serialize(recurring_snapshot_trigger_json)),
                                          std::make_tuple(recurring_snapshot_stop_test, serialize(recurring_snapshot_stop_json)),
                                          std::make_tuple(dummy_receiver, serialize(non_compliant_command_1)),
                                          std::make_tuple(dummy_receiver, serialize(non_compliant_command_2)),
                                          std::make_tuple(dummy_receiver, serialize(bad_acquire_command)),
                                          std::make_tuple(dummy_receiver, random_str1),
                                          std::make_tuple(dummy_receiver, random_str2),
                                          std::make_tuple(dummy_receiver, random_str3),
                                          std::make_tuple(dummy_receiver, random_str4),
                                          std::make_tuple(dummy_receiver, random_str5),
                                          std::make_tuple(dummy_receiver, random_str6),
                                          std::make_tuple(dummy_receiver, random_str7)));
#endif  // __linux__