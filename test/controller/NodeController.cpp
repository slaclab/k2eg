
#include <gtest/gtest.h>
#include <iostream>
#include <k2eg/common/ProgramOptions.h>
#include <k2eg/common/utility.h>
#include <k2eg/controller/command/cmd/Command.h>
#include <k2eg/controller/command/cmd/PutCommand.h>
#include <k2eg/controller/node/NodeController.h>
#include <k2eg/service/ServiceResolver.h>
#include <k2eg/service/configuration/configuration.h>
#include <k2eg/service/data/DataStorage.h>
#include <k2eg/service/epics/EpicsData.h>
#include <k2eg/service/epics/EpicsServiceManager.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/log/impl/BoostLogger.h>
#include <k2eg/service/metric/impl/prometheus/PrometheusMetricService.h>
#include <k2eg/service/pubsub/pubsub.h>
#include <k2eg/service/scheduler/Scheduler.h>

#include <boost/json.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <latch>
#include <memory>
#include <msgpack.hpp>
#include <ostream>
#include <random>
#include <ratio>
#include <string>
#include <thread>

#include "NodeControllerCommon.h"
#include "boost/json/object.hpp"
#include "k2eg/controller/command/cmd/MonitorCommand.h"
#include "k2eg/controller/command/cmd/SnapshotCommand.h"
#include "k2eg/service/metric/IMetricService.h"
#include "k2eg/service/pubsub/IPublisher.h"
#include "msgpack/v3/object_fwd_decl.hpp"

namespace bs = boost::system;
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
using namespace k2eg::service::scheduler;
using namespace k2eg::service::epics_impl;
using namespace k2eg::service::configuration;
using namespace k2eg::service::configuration::impl::consul;

using namespace k2eg::service::pubsub;
using namespace k2eg::service::pubsub::impl::kafka;

using namespace k2eg::service::metric;
using namespace k2eg::service::metric::impl::prometheus_impl;

#define KAFKA_HOSTNAME         "kafka:9092"
#define KAFKA_TOPIC_ACQUIRE_IN "acquire_commad_in"

int tcp_port = 9000;

#ifdef __linux__


TEST(NodeController, MonitorCommandJsonSerByDefault)
{
    std::latch                      work_done{2};
    boost::json::object             reply_msg;
    std::unique_ptr<NodeController> node_controller;
    auto                            publisher = std::make_shared<DummyPublisher>(work_done);
    node_controller = initBackend(tcp_port, publisher);

    // add the number of reader from topic
    dynamic_cast<ControllerConsumerDummyPublisher*>(publisher.get())->setConsumerNumber(1);
    while (!node_controller->isWorkerReady(k2eg::controller::command::cmd::CommandType::monitor))
    {
        sleep(1);
    }
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const MonitorCommand>(MonitorCommand{CommandType::monitor, SerializationType::JSON, KAFKA_TOPIC_ACQUIRE_IN, "rep-id", "pva://channel:ramp:ramp", KAFKA_TOPIC_ACQUIRE_IN})}););

    work_done.wait();
    // reduce the number of consumer
    dynamic_cast<ControllerConsumerDummyPublisher*>(publisher.get())->setConsumerNumber(0);
    // force call add purge timestamp to the monitor
    node_controller->performManagementTask();
    sleep(5);
    // this close the emonitor
    node_controller->performManagementTask();
    // we need to have publish some message
    size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_NE(published, 0);
    // check if we have received an event on the reply topic
    EXPECT_NO_THROW(reply_msg = exstractJsonObjectThatContainsKey(publisher->sent_messages, "channel:ramp:ramp", KAFKA_TOPIC_ACQUIRE_IN));
    EXPECT_EQ(reply_msg.contains("channel:ramp:ramp"), true);

    // chec that there is a reply
    EXPECT_NO_THROW(reply_msg = exstractJsonObjectThatContainsKey(publisher->sent_messages, KEY_REPLY_ID, KAFKA_TOPIC_ACQUIRE_IN));
    EXPECT_EQ(reply_msg.contains(KEY_REPLY_ID), true);
    // confirm that monitor has stopped
    EXPECT_NO_THROW(published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize(););
    sleep(2);
    EXPECT_EQ(ServiceResolver<IPublisher>::resolve()->getQueueMessageSize(), published);

    // check that we have json data
    EXPECT_NO_THROW(auto json_object = getJsonObject(*publisher->sent_messages[0]););

    // dispose all
    deinitBackend(std::move(node_controller));
}

TEST(NodeController, MonitorCommandJsonSerStalePV)
{
    // we have to wait for two monitor and two reply messages
    std::latch                      work_done{4};
    boost::json::object             reply_msg;
    std::unique_ptr<NodeController> node_controller;
    auto                            publisher = std::make_shared<DummyPublisher>(work_done);
    node_controller = initBackend(tcp_port, publisher);

    // add the number of reader from topic
    dynamic_cast<ControllerConsumerDummyPublisher*>(publisher.get())->setConsumerNumber(1);
    while (!node_controller->isWorkerReady(k2eg::controller::command::cmd::CommandType::monitor))
    {
        sleep(1);
    }
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const MonitorCommand>(MonitorCommand{CommandType::monitor, SerializationType::JSON, KAFKA_TOPIC_ACQUIRE_IN, "rep-id", "pva://variable:a", "variable_a"})}););
    sleep(2);
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const MonitorCommand>(MonitorCommand{CommandType::monitor, SerializationType::JSON, KAFKA_TOPIC_ACQUIRE_IN, "rep-id", "pva://variable:a", "variable_a"})}););
    sleep(2);
    work_done.wait();
    EXPECT_EQ(countMessageOnTopic(publisher->sent_messages, "variable_a"), 2);
    // reduce the number of consumer
    dynamic_cast<ControllerConsumerDummyPublisher*>(publisher.get())->setConsumerNumber(0);
    // force call add purge timestamp to the monitor
    node_controller->performManagementTask();
    sleep(5);
    // this close the emonitor
    node_controller->performManagementTask();
    // we need to have publish some message
    size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_NE(published, 0);
    // dispose all
    deinitBackend(std::move(node_controller));
}

TEST(NodeController, MonitorCommandSpecifySpecificMonitorEventTopic)
{
    std::latch                      work_done{2};
    boost::json::object             reply_msg;
    std::unique_ptr<NodeController> node_controller;
    auto                            publisher = std::make_shared<DummyPublisher>(work_done);
    node_controller = initBackend(tcp_port,publisher);
    dynamic_cast<ControllerConsumerDummyPublisher*>(publisher.get())->setConsumerNumber(1);
    while (!node_controller->isWorkerReady(k2eg::controller::command::cmd::CommandType::monitor))
    {
        sleep(1);
    }
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const MonitorCommand>(MonitorCommand{CommandType::monitor, SerializationType::JSON, KAFKA_TOPIC_ACQUIRE_IN, "rep-id", "pva://channel:ramp:ramp", "alternate_topic"})}););

    work_done.wait();

    // reduce the number of consumer
    dynamic_cast<ControllerConsumerDummyPublisher*>(publisher.get())->setConsumerNumber(0);
    // force call add purge timestamp to the monitor
    node_controller->performManagementTask();
    sleep(5);
    // this close the emonitor
    node_controller->performManagementTask();

    // we need to have publish some message
    size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_NE(published, 0);
    // check if we have received an event on the reply topic
    EXPECT_NO_THROW(reply_msg = exstractJsonObjectThatContainsKey(publisher->sent_messages, "channel:ramp:ramp", "alternate_topic"));
    EXPECT_EQ(reply_msg.contains("channel:ramp:ramp"), true);

    // chec that there is a reply
    EXPECT_NO_THROW(reply_msg = exstractJsonObjectThatContainsKey(publisher->sent_messages, KEY_REPLY_ID, KAFKA_TOPIC_ACQUIRE_IN));
    EXPECT_EQ(reply_msg.contains(KEY_REPLY_ID), true);

    // confirm that monitor is stopped
    EXPECT_NO_THROW(published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize(););
    sleep(2);
    EXPECT_EQ(ServiceResolver<IPublisher>::resolve()->getQueueMessageSize(), published);

    // check that we have json data
    EXPECT_NO_THROW(auto json_object = getJsonObject(*publisher->sent_messages[0]););
    // dispose all
    deinitBackend(std::move(node_controller));
}

TEST(NodeController, MonitorCommandMsgPackSer)
{
    std::latch                      work_done{2};
    msgpack::unpacked               reply_msg;
    std::unique_ptr<NodeController> node_controller;
    auto                            publisher = std::make_shared<DummyPublisher>(work_done);
    node_controller = initBackend(tcp_port,publisher);
    while (!node_controller->isWorkerReady(k2eg::controller::command::cmd::CommandType::monitor))
    {
        sleep(1);
    }
    dynamic_cast<ControllerConsumerDummyPublisher*>(publisher.get())->setConsumerNumber(1);
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const MonitorCommand>(MonitorCommand{CommandType::monitor, SerializationType::Msgpack, KAFKA_TOPIC_ACQUIRE_IN, "rep-id", "pva://channel:ramp:ramp", KAFKA_TOPIC_ACQUIRE_IN})}););

    work_done.wait();
    // reduce the number of consumer
    dynamic_cast<ControllerConsumerDummyPublisher*>(publisher.get())->setConsumerNumber(0);
    // force call add purge timestamp to the monitor
    node_controller->performManagementTask();
    sleep(5);
    // this close the emonitor
    node_controller->performManagementTask();

    // we need to have publish some message
    size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_NE(published, 0);

    // configrm that monitor has stoppped
    sleep(1);
    EXPECT_NO_THROW(published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize(););
    sleep(2);
    EXPECT_EQ(ServiceResolver<IPublisher>::resolve()->getQueueMessageSize(), published);

    // check that we have msgpack data
    msgpack::unpacked msgpack_unpacked;
    msgpack::object   msgpack_object;

    EXPECT_NO_THROW(msgpack_unpacked = exstractMsgpackObjectThatContainsKey(publisher->sent_messages, "channel:ramp:ramp", KAFKA_TOPIC_ACQUIRE_IN));

    // EXPECT_NO_THROW(msgpack_unpacked = getMsgPackObject(*publisher->sent_messages[0]););
    msgpack_object = msgpack_unpacked.get();
    EXPECT_EQ(msgpack_object.type, msgpack::type::MAP);
    // dispose all
    deinitBackend(std::move(node_controller));
}

TEST(NodeController, MonitorCommandMsgPackCompactSer)
{
    std::latch                      work_done{2};
    std::unique_ptr<NodeController> node_controller;
    auto                            publisher = std::make_shared<DummyPublisher>(work_done);
    node_controller = initBackend(tcp_port,publisher);
    while (!node_controller->isWorkerReady(k2eg::controller::command::cmd::CommandType::monitor))
    {
        sleep(1);
    }
    dynamic_cast<ControllerConsumerDummyPublisher*>(publisher.get())->setConsumerNumber(1);
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const MonitorCommand>(MonitorCommand{CommandType::monitor, SerializationType::MsgpackCompact, KAFKA_TOPIC_ACQUIRE_IN, "rep-id", "pva://channel:ramp:ramp", KAFKA_TOPIC_ACQUIRE_IN})}););

    work_done.wait();
    // stop acquire
    // reduce the number of consumer
    dynamic_cast<ControllerConsumerDummyPublisher*>(publisher.get())->setConsumerNumber(0);
    // force call add purge timestamp to the monitor
    node_controller->performManagementTask();
    sleep(5);
    // this close the emonitor
    node_controller->performManagementTask();

    // we need to have publish some message
    size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_NE(published, 0);

    // confirm that monitor has stoppped
    sleep(1);
    EXPECT_NO_THROW(published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize(););
    sleep(2);
    EXPECT_EQ(ServiceResolver<IPublisher>::resolve()->getQueueMessageSize(), published);

    // check that we have msgpack compact
    msgpack::unpacked msgpack_unpacked;
    msgpack::object   msgpack_object;
    // EXPECT_NO_THROW(msgpack_unpacked = getMsgPackObject(*publisher->sent_messages[0]););
    EXPECT_NO_THROW(msgpack_unpacked = exstractMsgpackObjectThatContainsKey(publisher->sent_messages, "channel:ramp:ramp", KAFKA_TOPIC_ACQUIRE_IN));
    msgpack_object = msgpack_unpacked.get();
    EXPECT_EQ(msgpack_object.type, msgpack::type::ARRAY);
    // dispose all
    deinitBackend(std::move(node_controller));
}

TEST(NodeController, MonitorCommandAfterReboot)
{
    std::latch work_done{2};
    std::latch work_done_2{5};
    auto       publisher = std::make_shared<DummyPublisher>(work_done);
    auto       node_controller = initBackend(tcp_port,publisher);

    while (!node_controller->isWorkerReady(k2eg::controller::command::cmd::CommandType::monitor))
    {
        sleep(1);
    }

    dynamic_cast<ControllerConsumerDummyPublisher*>(publisher.get())->setConsumerNumber(1);
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const MonitorCommand>(MonitorCommand{CommandType::monitor, SerializationType::JSON, KAFKA_TOPIC_ACQUIRE_IN, "rep-id", "pva://channel:ramp:ramp", KAFKA_TOPIC_ACQUIRE_IN})}););

    work_done.wait();
    // we need to have publish some message
    size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_NE(published, 0);

    // stop the node controller
    deinitBackend(std::move(node_controller));

    // reboot without delete database (by defauilt the initBackend reset alwasy the configuration)
    node_controller = initBackend(tcp_port, std::make_shared<DummyPublisher>(work_done_2), true, false);
    dynamic_cast<ControllerConsumerDummyPublisher*>(publisher.get())->setConsumerNumber(1);
    // we have to wait for monitor event
    work_done_2.wait();
    // we need to have publish some message
    published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_NE(published, 0);

    // dispose all
    deinitBackend(std::move(node_controller));
}

TEST(NodeController, MonitorCommandMultiPV)
{
    std::latch                      work_done{3};
    boost::json::object             reply_msg;
    std::unique_ptr<NodeController> node_controller;
    auto                            publisher = std::make_shared<DummyPublisher>(work_done);
    node_controller = initBackend(tcp_port, publisher);

    // add the number of reader from topic
    dynamic_cast<ControllerConsumerDummyPublisher*>(publisher.get())->setConsumerNumber(1);
    while (!node_controller->isWorkerReady(k2eg::controller::command::cmd::CommandType::monitor))
    {
        sleep(1);
    }
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const MultiMonitorCommand>(MultiMonitorCommand{
        CommandType::multi_monitor,
        SerializationType::JSON,
        KAFKA_TOPIC_ACQUIRE_IN,
        "rep-id",
        {"pva://variable:a", "pva://variable:b"},
    })}););

    work_done.wait();
    // reduce the number of consumer
    dynamic_cast<ControllerConsumerDummyPublisher*>(publisher.get())->setConsumerNumber(0);
    // force call add purge timestamp to the monitor
    node_controller->performManagementTask();
    sleep(5);
    // this close the emonitor
    node_controller->performManagementTask();
    // we need to have publish some message
    size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_NE(published, 0);
    // check if we have received an event for 'variable:a' pv on 'variable_a' topic
    EXPECT_NO_THROW(reply_msg = exstractJsonObjectThatContainsKey(publisher->sent_messages, "variable:a", "variable_a"));
    EXPECT_EQ(reply_msg.contains("variable:a"), true);

    // check if we have received an event for 'variable:b' pv on 'variable_b' topic
    EXPECT_NO_THROW(reply_msg = exstractJsonObjectThatContainsKey(publisher->sent_messages, "variable:b", "variable_b"));
    EXPECT_EQ(reply_msg.contains("variable:b"), true);
    // confirm that monitor has stopped
    EXPECT_NO_THROW(published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize(););
    sleep(2);
    EXPECT_EQ(ServiceResolver<IPublisher>::resolve()->getQueueMessageSize(), published);
    // dispose all
    deinitBackend(std::move(node_controller));
}

TEST(NodeController, MonitorCommandMultiPVStress)
{
    boost::json::object             reply_msg;
    std::unique_ptr<NodeController> node_controller;
    std::vector<std::string>        topics = {"channel_ramp_ramp", "channel_ramp_ramp_1"};
    auto                            publisher = std::make_shared<TopicTargetPublisher>(topics);
    node_controller = initBackend(tcp_port, publisher, true);
    publisher->enable_log = true;
    // add the number of reader from topic
    dynamic_cast<ControllerConsumerDummyPublisher*>(publisher.get())->setConsumerNumber(1);
    while (!node_controller->isWorkerReady(k2eg::controller::command::cmd::CommandType::monitor))
    {
        sleep(1);
    }
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const MultiMonitorCommand>(MultiMonitorCommand{
        CommandType::multi_monitor,
        SerializationType::JSON,
        KAFKA_TOPIC_ACQUIRE_IN,
        "rep-id",
        {"pva://channel:ramp:ramp",    "pva://channel:ramp:ramp_1",  "pva://channel:ramp:ramp_2",
         "pva://channel:ramp:ramp_3",  "pva://channel:ramp:ramp_4",  "pva://channel:ramp:ramp_5",
         "pva://channel:ramp:ramp_6",  "pva://channel:ramp:ramp_7",  "pva://channel:ramp:ramp_8",
         "pva://channel:ramp:ramp_9",  "pva://channel:ramp:ramp_10", "pva://channel:ramp:ramp_11",
         "pva://channel:ramp:ramp_12", "pva://channel:ramp:ramp_13", "pva://channel:ramp:ramp_14",
         "pva://channel:ramp:ramp_15", "pva://channel:ramp:ramp_16", "pva://channel:ramp:ramp_17",
         "pva://channel:ramp:ramp_18", "pva://channel:ramp:ramp_19", "pva://channel:ramp:ramp_20",
         "pva://channel:ramp:ramp_21", "pva://channel:ramp:ramp_22", "pva://channel:ramp:ramp_23",
         "pva://channel:ramp:ramp_24", "pva://channel:ramp:ramp_25", "pva://channel:ramp:ramp_26",
         "pva://channel:ramp:ramp_27", "pva://channel:ramp:ramp_28", "pva://channel:ramp:ramp_29",
         "pva://channel:ramp:ramp_30", "pva://channel:ramp:ramp_31", "pva://channel:ramp:ramp_32",
         "pva://channel:ramp:ramp_33", "pva://channel:ramp:ramp_34", "pva://channel:ramp:ramp_35",
         "pva://channel:ramp:ramp_36", "pva://channel:ramp:ramp_37", "pva://channel:ramp:ramp_38",
         "pva://channel:ramp:ramp_39"},
    })}););

    publisher->getLatch().wait();
    // reduce the number of consumer
    dynamic_cast<ControllerConsumerDummyPublisher*>(publisher.get())->setConsumerNumber(0);
    // force call add purge timestamp to the monitor
    node_controller->performManagementTask();
    sleep(2);
    // this close the emonitor
    node_controller->performManagementTask();
    // we need to have publish some message
    size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_NE(published, 0);
    // check if we have received an event for ramp
    EXPECT_NO_THROW(reply_msg = exstractJsonObjectThatContainsKey(publisher->sent_messages, "channel:ramp:ramp", "channel_ramp_ramp"));
    EXPECT_EQ(reply_msg.contains("channel:ramp:ramp"), true);

    // check if we have received an event for ramp_1
    EXPECT_NO_THROW(reply_msg = exstractJsonObjectThatContainsKey(publisher->sent_messages, "channel:ramp:ramp_1", "channel_ramp_ramp_1"));
    EXPECT_EQ(reply_msg.contains("channel:ramp:ramp_1"), true);
    // dispose all
    deinitBackend(std::move(node_controller));
}

TEST(NodeController, GetCommandJson)
{
    boost::json::object             json_object;
    std::latch                      work_done{1};
    std::shared_ptr<DummyPublisher> publisher = std::make_shared<DummyPublisher>(work_done);
    // set environment variable for test
    auto node_controller = initBackend(tcp_port, publisher);

    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const GetCommand>(GetCommand{CommandType::get, SerializationType::JSON, KAFKA_TOPIC_ACQUIRE_IN, "id", "pva://channel:ramp:ramp"})}););

    work_done.wait();
    // we need to have publish some message
    size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_NE(published, 0);
    // check for json forward
    EXPECT_NO_THROW(json_object = getJsonObject(*publisher->sent_messages[0]););
    EXPECT_EQ(json_object.contains("error"), true);
    EXPECT_EQ(json_object.contains(KEY_REPLY_ID), true);
    EXPECT_EQ(json_object.contains("channel:ramp:ramp"), true);
    // dispose all
    deinitBackend(std::move(node_controller));
}

TEST(NodeController, GetCommandJsonWithReplyID)
{
    boost::json::object             json_obj;
    std::latch                      work_done{1};
    std::shared_ptr<DummyPublisher> publisher = std::make_shared<DummyPublisher>(work_done);
    // set environment variable for test
    auto node_controller = initBackend(tcp_port, publisher);

    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const GetCommand>(GetCommand{CommandType::get, SerializationType::JSON, KAFKA_TOPIC_ACQUIRE_IN, "REP_ID_JSON", "pva://channel:ramp:ramp"})}););

    work_done.wait();
    // we need to have publish some message
    size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_NE(published, 0);
    // check for json forward
    EXPECT_NO_THROW(json_obj = getJsonObject(*publisher->sent_messages[0]););
    EXPECT_EQ(json_obj.contains("error"), true);
    EXPECT_EQ(json_obj.contains(KEY_REPLY_ID), true);
    EXPECT_EQ(json_obj.contains("channel:ramp:ramp"), true);
    EXPECT_STREQ(json_obj[KEY_REPLY_ID].as_string().c_str(), "REP_ID_JSON");
    // dispose all
    deinitBackend(std::move(node_controller));
}

TEST(NodeController, GetFaultyCommandJsonWithReplyID)
{
    boost::json::object             json_obj;
    std::latch                      work_done{1};
    std::shared_ptr<DummyPublisher> publisher = std::make_shared<DummyPublisher>(work_done);
    // set environment variable for test
    auto node_controller = initBackend(tcp_port, publisher);

    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const GetCommand>(GetCommand{CommandType::get, SerializationType::JSON, KAFKA_TOPIC_ACQUIRE_IN, "REP_ID_JSON", "pva://bad:channel"})}););

    work_done.wait();
    // we need to have publish some message
    size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_NE(published, 0);
    // check for json forward
    EXPECT_NO_THROW(json_obj = getJsonObject(*publisher->sent_messages[0]););
    EXPECT_EQ(json_obj.contains("error"), true);
    EXPECT_NE(json_obj["error"].as_int64(), 0);
    EXPECT_EQ(json_obj.contains(KEY_REPLY_ID), true);
    EXPECT_EQ(json_obj.contains("message"), true);
    EXPECT_STREQ(json_obj[KEY_REPLY_ID].as_string().c_str(), "REP_ID_JSON");
    // dispose all
    deinitBackend(std::move(node_controller));
}

TEST(NodeController, GetCommandMsgPack)
{
    typedef std::map<std::string, msgpack::object> Map;
    std::latch                                     work_done{1};
    std::shared_ptr<DummyPublisher>                publisher = std::make_shared<DummyPublisher>(work_done);
    // set environment variable for test
    auto node_controller = initBackend(tcp_port,publisher);

    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const GetCommand>(GetCommand{CommandType::get, SerializationType::Msgpack, KAFKA_TOPIC_ACQUIRE_IN, "id", "pva://channel:ramp:ramp"})}););

    work_done.wait();
    // we need to have publish some message
    size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_NE(published, 0);
    // check for msgpack map
    msgpack::unpacked msgpack_unpacked;
    msgpack::object   msgpack_object;
    EXPECT_NO_THROW(msgpack_unpacked = getMsgPackObject(*publisher->sent_messages[0]););
    EXPECT_NO_THROW(msgpack_object = msgpack_unpacked.get(););
    EXPECT_EQ(msgpack_object.type, msgpack::type::MAP);
    auto map_reply = msgpack_object.as<Map>();
    EXPECT_EQ(map_reply.contains("error"), true);
    EXPECT_EQ(map_reply.contains(KEY_REPLY_ID), true);
    EXPECT_EQ(map_reply.contains("channel:ramp:ramp"), true);
    // dispose all
    deinitBackend(std::move(node_controller));
}

TEST(NodeController, GetFaultyCommandMsgPack)
{
    typedef std::map<std::string, msgpack::object> Map;
    std::latch                                     work_done{1};
    std::shared_ptr<DummyPublisher>                publisher = std::make_shared<DummyPublisher>(work_done);
    // set environment variable for test
    auto node_controller = initBackend(tcp_port,publisher);

    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const GetCommand>(GetCommand{CommandType::get, SerializationType::Msgpack, KAFKA_TOPIC_ACQUIRE_IN, "id", "pva://bad:pv:name"})}););

    work_done.wait();
    // we need to have publish some message
    size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_NE(published, 0);
    // check for msgpack map
    msgpack::unpacked msgpack_unpacked;
    msgpack::object   msgpack_object;
    EXPECT_NO_THROW(msgpack_unpacked = getMsgPackObject(*publisher->sent_messages[0]););
    EXPECT_NO_THROW(msgpack_object = msgpack_unpacked.get(););
    EXPECT_EQ(msgpack_object.type, msgpack::type::MAP);
    auto map_reply = msgpack_object.as<Map>();
    EXPECT_EQ(map_reply.contains("error"), true);
    EXPECT_EQ(map_reply.contains(KEY_REPLY_ID), true);
    EXPECT_EQ(map_reply.contains("message"), true);
    EXPECT_EQ(map_reply.at("message").type, msgpack::type::STR);
    // dispose all
    deinitBackend(std::move(node_controller));
}

TEST(NodeController, GetCommandMsgPackReplyID)
{
    typedef std::map<std::string, msgpack::object> Map;
    std::latch                                     work_done{1};
    std::shared_ptr<DummyPublisher>                publisher = std::make_shared<DummyPublisher>(work_done);
    // set environment variable for test
    auto node_controller = initBackend(tcp_port,publisher);

    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const GetCommand>(GetCommand{CommandType::get, SerializationType::MsgpackCompact, KAFKA_TOPIC_ACQUIRE_IN, "REPLY_ID_MSGPACK", "pva://channel:ramp:ramp"})}););

    work_done.wait();
    // we need to have publish some message
    size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_NE(published, 0);
    // check for msgpack map
    msgpack::unpacked msgpack_unpacked;
    msgpack::object   msgpack_object;
    EXPECT_NO_THROW(msgpack_unpacked = getMsgPackObject(*publisher->sent_messages[0]););
    EXPECT_NO_THROW(msgpack_object = msgpack_unpacked.get(););
    EXPECT_EQ(msgpack_object.type, msgpack::type::MAP);
    auto map_reply = msgpack_object.as<Map>();
    EXPECT_EQ(map_reply.contains("error"), true);
    EXPECT_EQ(map_reply.contains(KEY_REPLY_ID), true);
    EXPECT_EQ(map_reply.contains("channel:ramp:ramp"), true);
    EXPECT_EQ(map_reply.at("channel:ramp:ramp").type, msgpack::type::ARRAY);
    EXPECT_STREQ(map_reply[KEY_REPLY_ID].as<std::string>().c_str(), "REPLY_ID_MSGPACK");
    // dispose all
    deinitBackend(std::move(node_controller));
}

TEST(NodeController, GetCommandMsgPackCompack)
{
    typedef std::map<std::string, msgpack::object> Map;
    std::latch                                     work_done{1};
    std::shared_ptr<DummyPublisher>                publisher = std::make_shared<DummyPublisher>(work_done);
    // set environment variable for test
    auto node_controller = initBackend(tcp_port,publisher);

    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const GetCommand>(GetCommand{CommandType::get, SerializationType::MsgpackCompact, KAFKA_TOPIC_ACQUIRE_IN, "id", "pva://channel:ramp:ramp"})}););

    work_done.wait();
    // we need to have publish some message
    size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_NE(published, 0);
    // check for masgpack compact array
    msgpack::unpacked msgpack_unpacked;
    msgpack::object   msgpack_object;
    EXPECT_NO_THROW(msgpack_unpacked = getMsgPackObject(*publisher->sent_messages[0]););
    EXPECT_NO_THROW(msgpack_object = msgpack_unpacked.get(););
    EXPECT_EQ(msgpack_object.type, msgpack::type::MAP);
    auto map_reply = msgpack_object.as<Map>();
    EXPECT_EQ(map_reply.contains("error"), true);
    EXPECT_EQ(map_reply.contains(KEY_REPLY_ID), true);
    EXPECT_EQ(map_reply.contains("channel:ramp:ramp"), true);
    EXPECT_EQ(map_reply.at("channel:ramp:ramp").type, msgpack::type::ARRAY);
    // dispose all
    deinitBackend(std::move(node_controller));
}

TEST(NodeController, GetFaultyCommandMsgPackCompack)
{
    typedef std::map<std::string, msgpack::object> Map;
    std::latch                                     work_done{1};
    std::shared_ptr<DummyPublisher>                publisher = std::make_shared<DummyPublisher>(work_done);
    // set environment variable for test
    auto node_controller = initBackend(tcp_port,publisher);

    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const GetCommand>(GetCommand{CommandType::get, SerializationType::MsgpackCompact, KAFKA_TOPIC_ACQUIRE_IN, "id", "pva://bad:pv:name"})}););

    work_done.wait();
    // we need to have publish some message
    size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_NE(published, 0);
    // check for masgpack compact array
    msgpack::unpacked msgpack_unpacked;
    msgpack::object   msgpack_object;
    EXPECT_NO_THROW(msgpack_unpacked = getMsgPackObject(*publisher->sent_messages[0]););
    EXPECT_NO_THROW(msgpack_object = msgpack_unpacked.get(););
    EXPECT_EQ(msgpack_object.type, msgpack::type::MAP);
    auto map_reply = msgpack_object.as<Map>();
    EXPECT_EQ(map_reply.contains("error"), true);
    EXPECT_EQ(map_reply.contains(KEY_REPLY_ID), true);
    EXPECT_EQ(map_reply.contains("message"), true);
    EXPECT_EQ(map_reply.at("message").type, msgpack::type::STR);
    // dispose all
    deinitBackend(std::move(node_controller));
}

TEST(NodeController, GetCommandMsgPackCompackWithReplyID)
{
    typedef std::map<std::string, msgpack::object> Map;
    typedef std::vector<msgpack::object>           VecTest;
    std::latch                                     work_done{1};
    std::shared_ptr<DummyPublisher>                publisher = std::make_shared<DummyPublisher>(work_done);
    // set environment variable for test
    auto node_controller = initBackend(tcp_port,publisher);

    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const GetCommand>(GetCommand{CommandType::get, SerializationType::MsgpackCompact, KAFKA_TOPIC_ACQUIRE_IN, "REPLY_ID_MSGPACK_COMPACT", "pva://channel:ramp:ramp"})}););

    work_done.wait();
    // we need to have publish some message
    size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_NE(published, 0);
    // check for masgpack compact array
    msgpack::unpacked msgpack_unpacked;
    msgpack::object   msgpack_object;
    EXPECT_NO_THROW(msgpack_unpacked = getMsgPackObject(*publisher->sent_messages[0]););
    msgpack_object = msgpack_unpacked.get();
    EXPECT_EQ(msgpack_object.type, msgpack::type::MAP);
    auto map_reply = msgpack_object.as<Map>();
    EXPECT_EQ(map_reply.contains("error"), true);
    EXPECT_EQ(map_reply.contains(KEY_REPLY_ID), true);
    EXPECT_EQ(map_reply.contains("channel:ramp:ramp"), true);
    EXPECT_EQ(map_reply.at("channel:ramp:ramp").type, msgpack::type::ARRAY);
    EXPECT_STREQ(map_reply[KEY_REPLY_ID].as<std::string>().c_str(), "REPLY_ID_MSGPACK_COMPACT");
    // dispose all
    deinitBackend(std::move(node_controller));
}

inline void wait_latch(std::latch& l)
{
    bool w = true;
    int  counter = 10;
    while (w && counter > 0)
    {
        if (l.try_wait())
        {
            w = false;
        }
        else
        {
            counter--;
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }
}

TEST(NodeController, GetCommandCAChannel)
{
    auto publisher = std::make_shared<DummyPublisherCounter>(1);
    // set environment variable for test
    auto node_controller = initBackend(tcp_port,publisher);

    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const GetCommand>(GetCommand{CommandType::get, SerializationType::JSON, KAFKA_TOPIC_ACQUIRE_IN, "id", "ca://variable:sum"})}););
    // give some time for the timeout
    wait_latch(publisher->l);
    // we need to have publish some message
    size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_NE(published, 0);

    // dispose all
    deinitBackend(std::move(node_controller));
}

TEST(NodeController, GetCommandBadChannel)
{
    std::latch work_done{1};
    // set environment variable for test
    auto node_controller = initBackend(tcp_port,std::make_shared<DummyPublisher>(work_done));

    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const GetCommand>(GetCommand{CommandType::get, SerializationType::JSON, KAFKA_TOPIC_ACQUIRE_IN, "id", "pva://bad:channel:name"})}););
    // give some time for the timeout
    sleep(5);
    // we need to have publish some message
    size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_EQ(published, 0);

    // dispose all
    deinitBackend(std::move(node_controller));
}

TEST(NodeController, PutCommandBadChannel)
{
    std::latch work_done{1};
    // set environment variable for test
    auto node_controller = initBackend(tcp_port,std::make_shared<DummyPublisher>(work_done));

    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const PutCommand>(PutCommand{CommandType::put, SerializationType::Unknown, KAFKA_TOPIC_ACQUIRE_IN, "id", "pva://bad:channel:name", "1"})}););

    // this should give the timeout of the put command so the node controller will exit without problem

    // dispose all
    deinitBackend(std::move(node_controller));
}

TEST(NodeController, PutCommandScalar)
{
    typedef std::map<std::string, msgpack::object> Map;
    typedef std::vector<msgpack::object>           Vec;
    std::random_device                             r;
    std::default_random_engine                     e1(r());
    std::uniform_int_distribution<int>             uniform_dist(1, 100);
    ConstChannelDataUPtr                           value_readout;
    msgpack::unpacked                              msgpack_unpacked;
    msgpack::object                                msgpack_object;
    auto                                           publisher = std::make_shared<DummyPublisherNoSignal>();
    // set environment variable for test
    auto node_controller = initBackend(tcp_port, publisher);
    auto random_scalar = uniform_dist(e1);
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const PutCommand>(PutCommand{CommandType::put, SerializationType::Msgpack, "reply-topic", "PUT_REPLY_ID", "pva://variable:b", std::to_string(random_scalar)})}););
    // give some time for the timeout
    wait_forPublished_message_size(*publisher, 1, 2000);

    // check for put reply
    EXPECT_EQ(publisher->sent_messages.size(), 1);
    EXPECT_NO_THROW(msgpack_unpacked = getMsgPackObject(*publisher->sent_messages[0]););
    msgpack_object = msgpack_unpacked.get();
    EXPECT_EQ(msgpack_object.type, msgpack::type::MAP);
    auto map_reply = msgpack_object.as<Map>();
    EXPECT_EQ(map_reply.contains("error"), true);
    EXPECT_EQ(map_reply["error"].as<int>(), 0);
    EXPECT_EQ(map_reply.contains(KEY_REPLY_ID), true);
    EXPECT_STREQ(map_reply[KEY_REPLY_ID].as<std::string>().c_str(), "PUT_REPLY_ID");

    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const GetCommand>(GetCommand{CommandType::get, SerializationType::MsgpackCompact, KAFKA_TOPIC_ACQUIRE_IN, "id", "pva://variable:b"})}););
    wait_forPublished_message_size(*publisher, 2, 200000);
    EXPECT_EQ(publisher->sent_messages.size(), 2);

    // check for get reply
    EXPECT_NO_THROW(msgpack_unpacked = getMsgPackObject(*publisher->sent_messages[1]););
    msgpack_object = msgpack_unpacked.get();
    EXPECT_EQ(msgpack_object.type, msgpack::type::MAP);
    map_reply = msgpack_object.as<Map>();
    EXPECT_EQ(map_reply.contains("variable:b"), true);
    EXPECT_EQ(map_reply["variable:b"].type, msgpack::type::ARRAY);
    auto vec_reply = map_reply["variable:b"].as<Vec>();
    EXPECT_EQ(vec_reply[0].type, msgpack::type::FLOAT);
    EXPECT_EQ(vec_reply[0].as<double>(), random_scalar);
    // dispose all
    deinitBackend(std::move(node_controller));
}

TEST(NodeController, PutCommandScalarArray)
{
    typedef std::map<std::string, msgpack::object> Map;
    typedef std::vector<msgpack::object>           Vec;
    msgpack::unpacked                              msgpack_unpacked;
    msgpack::object                                msgpack_object;
    ConstChannelDataUPtr                           value_readout;
    auto                                           publisher = std::make_shared<DummyPublisherNoSignal>();
    // set environment variable for test
    auto node_controller = initBackend(tcp_port,publisher);

    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const PutCommand>(PutCommand{CommandType::put, SerializationType::MsgpackCompact, "DESTINATION_TOPIC", "PUT_REPLY_ID", "pva://channel:waveform", "8 0 0 0 0 0 0 0 0"})}););
    // give some time for the timeout
    wait_forPublished_message_size(*publisher, 1, 2000);
    EXPECT_EQ(publisher->sent_messages.size(), 1);
    EXPECT_NO_THROW(msgpack_unpacked = getMsgPackObject(*publisher->sent_messages[0]););
    msgpack_object = msgpack_unpacked.get();
    EXPECT_EQ(msgpack_object.type, msgpack::type::MAP);
    auto map_reply = msgpack_object.as<Map>();
    EXPECT_EQ(map_reply.contains("error"), true);
    EXPECT_EQ(map_reply["error"].as<int>(), 0);
    EXPECT_EQ(map_reply.contains(KEY_REPLY_ID), true);
    EXPECT_STREQ(map_reply[KEY_REPLY_ID].as<std::string>().c_str(), "PUT_REPLY_ID");

    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const GetCommand>(GetCommand{CommandType::get, SerializationType::MsgpackCompact, KAFKA_TOPIC_ACQUIRE_IN, "", "pva://channel:waveform"})}););
    wait_forPublished_message_size(*publisher, 2, 2000);
    EXPECT_EQ(publisher->sent_messages.size(), 2);
    // wait for the result of get command

    EXPECT_NO_THROW(msgpack_unpacked = getMsgPackObject(*publisher->sent_messages[1]););
    msgpack_object = msgpack_unpacked.get();
    EXPECT_EQ(msgpack_object.type, msgpack::type::MAP);
    map_reply = msgpack_object.as<Map>();
    EXPECT_EQ(map_reply.contains("channel:waveform"), true);
    EXPECT_EQ(map_reply["channel:waveform"].type, msgpack::type::ARRAY);
    auto vec_reply = map_reply["channel:waveform"].as<Vec>();
    EXPECT_EQ(vec_reply[0].type, msgpack::type::ARRAY);
    // dispose all
    deinitBackend(std::move(node_controller));
}

TEST(NodeController, PutCommandOnWrongPVCheckReply)
{
    typedef std::map<std::string, msgpack::object> Map;
    typedef std::vector<msgpack::object>           Vec;
    msgpack::unpacked                              msgpack_unpacked;
    msgpack::object                                msgpack_object;
    ConstChannelDataUPtr                           value_readout;
    auto                                           publisher = std::make_shared<DummyPublisherNoSignal>();
    // set environment variable for test
    auto node_controller = initBackend(tcp_port,publisher);

    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const PutCommand>(PutCommand{CommandType::put, SerializationType::MsgpackCompact, "DESTINATION_TOPIC", "PUT_REPLY_ID", "pva://channel:wrong_pv_name", "8 0 0 0 0 0 0 0 0"})}););
    // give some time for the timeout
    wait_forPublished_message_size(*publisher, 1, 10000);
    EXPECT_EQ(publisher->sent_messages.size(), 1);
    EXPECT_NO_THROW(msgpack_unpacked = getMsgPackObject(*publisher->sent_messages[0]););
    msgpack_object = msgpack_unpacked.get();
    EXPECT_EQ(msgpack_object.type, msgpack::type::MAP);
    auto map_reply = msgpack_object.as<Map>();
    EXPECT_EQ(map_reply.contains("error"), true);
    EXPECT_EQ(map_reply["error"].as<int>(), -3);
    EXPECT_EQ(map_reply.contains(KEY_REPLY_ID), true);
    EXPECT_STREQ(map_reply[KEY_REPLY_ID].as<std::string>().c_str(), "PUT_REPLY_ID");
    EXPECT_EQ(map_reply.contains("message"), true);
    EXPECT_NE(map_reply["message"].as<std::string>().size(), 0);
    // dispose all
    deinitBackend(std::move(node_controller));
}

TEST(NodeController, PutCommandMultithreadCheck)
{
    typedef std::map<std::string, msgpack::object> Map;
    typedef std::vector<msgpack::object>           Vec;
    msgpack::unpacked                              msgpack_unpacked;
    msgpack::object                                msgpack_object;
    ConstChannelDataUPtr                           value_readout;
    auto                                           publisher = std::make_shared<DummyPublisherNoSignal>();
    // set environment variable for test
    auto node_controller = initBackend(tcp_port,publisher);

    // this should wait the tiemout
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const PutCommand>(PutCommand{CommandType::put, SerializationType::MsgpackCompact, "DESTINATION_TOPIC", "PUT_REPLY_ID_1", "pva://channel:wrong_pv_name", "8 0 0 0 0 0 0 0 0"})}););
    // this should coplete first
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const PutCommand>(PutCommand{CommandType::put, SerializationType::MsgpackCompact, "DESTINATION_TOPIC", "PUT_REPLY_ID_2", "pva://variable:b", "1"})}););
    // give some time for the timeout
    wait_forPublished_message_size(*publisher, 2, 100000);
    EXPECT_EQ(publisher->sent_messages.size(), 2);

    // the fisr completed should be the second one
    EXPECT_NO_THROW(msgpack_unpacked = getMsgPackObject(*publisher->sent_messages[0]););
    msgpack_object = msgpack_unpacked.get();
    EXPECT_EQ(msgpack_object.type, msgpack::type::MAP);
    auto map_reply = msgpack_object.as<Map>();
    EXPECT_EQ(map_reply.contains(KEY_REPLY_ID), true);
    EXPECT_STREQ(map_reply[KEY_REPLY_ID].as<std::string>().c_str(), "PUT_REPLY_ID_2");

    EXPECT_NO_THROW(msgpack_unpacked = getMsgPackObject(*publisher->sent_messages[1]););
    msgpack_object = msgpack_unpacked.get();
    EXPECT_EQ(msgpack_object.type, msgpack::type::MAP);
    map_reply = msgpack_object.as<Map>();
    EXPECT_EQ(map_reply.contains("error"), true);
    EXPECT_EQ(map_reply["error"].as<int>(), -3);
    EXPECT_EQ(map_reply.contains(KEY_REPLY_ID), true);
    EXPECT_STREQ(map_reply[KEY_REPLY_ID].as<std::string>().c_str(), "PUT_REPLY_ID_1");

    // dispose all
    deinitBackend(std::move(node_controller));
}

#endif // __linux__
TEST(NodeController, RandomCommand)
{
    std::random_device                 r;
    std::default_random_engine         e1(r());
    std::uniform_int_distribution<int> uniform_dist(0, 2);
    std::uniform_int_distribution<int> uniform_dist_sleep(500, 1000);
    // set environment variable for test
    auto node_controller = initBackend(tcp_port,std::make_shared<DummyPublisherNoSignal>());

    // send 100 random commands equence iteration
    for (int idx = 0; idx < 100; idx++)
    {
        int rand_selection = uniform_dist(e1);
        std::cout << "[ RUN      ] test:" << idx << " random index:" << rand_selection << std::endl;
        switch (rand_selection)
        {
        case 0:
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(uniform_dist_sleep(e1)));
                EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const MonitorCommand>(MonitorCommand{CommandType::monitor, SerializationType::JSON, "", "", "pva://channel:ramp:ramp"})}););
                break;
            }
        case 1:
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(uniform_dist_sleep(e1)));
                EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const GetCommand>(GetCommand{CommandType::get, SerializationType::JSON, KAFKA_TOPIC_ACQUIRE_IN, "", "pva://variable:b"})}););
                break;
            }

        case 2:
            {
                auto random_scalar = uniform_dist(e1);
                std::this_thread::sleep_for(std::chrono::milliseconds(uniform_dist_sleep(e1)));
                EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const PutCommand>(PutCommand{CommandType::put, SerializationType::Unknown, "", "", "pva://variable:b", std::to_string(random_scalar)})}););
            }
        }
    }
    node_controller->waitForTaskCompletion();
    // dispose all
    deinitBackend(std::move(node_controller));
}