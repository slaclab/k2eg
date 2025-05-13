
#include "NodeControllerCommon.h"

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
#include <memory>
#include <msgpack.hpp>
#include <string>
#include <thread>
#include <unistd.h>

#include "NodeControllerCommon.h"

#include "k2eg/controller/command/cmd/MonitorCommand.h"
#include "k2eg/controller/command/cmd/SnapshotCommand.h"
#include "k2eg/service/pubsub/IPublisher.h"
#include "msgpack/v3/object_fwd_decl.hpp"

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

int ncs_tcp_port = 9000;

TEST(NodeControllerSnapshot, SnapshotCommandMsgPackSer)
{

    typedef std::map<std::string, msgpack::object> Map;
    std::latch                                     work_done{3};
    boost::json::object                            reply_msg;
    std::unique_ptr<NodeController>                node_controller;
    auto                                           publisher = std::make_shared<DummyPublisher>(work_done);
    node_controller = initBackend(ncs_tcp_port, publisher);

    // add the number of reader from topic
    dynamic_cast<ControllerConsumerDummyPublisher*>(publisher.get())->setConsumerNumber(1);
    while (!node_controller->isWorkerReady(k2eg::controller::command::cmd::CommandType::snapshot))
    {
        sleep(1);
    }

    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const SnapshotCommand>(SnapshotCommand{CommandType::snapshot, SerializationType::Msgpack, KAFKA_TOPIC_ACQUIRE_IN, "rep-id", {"pva://variable:a", "pva://variable:b"}, 1000})}););

    work_done.wait();

    // we need to have publish some message
    size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_EQ(published, 3); // two values and one completion message

    msgpack::unpacked msgpack_unpacked;
    msgpack::object   msgpack_object;

    // get first value could be one for variable a or b
    EXPECT_NO_THROW(msgpack_unpacked = getMsgPackObject(*publisher->sent_messages[0]););
    msgpack_object = msgpack_unpacked.get();
    EXPECT_EQ(msgpack_object.type, msgpack::type::MAP);
    auto map_reply = msgpack_object.as<Map>();
    EXPECT_EQ(map_reply.contains("error"), true);
    EXPECT_EQ(map_reply.contains(KEY_REPLY_ID), true);
    EXPECT_TRUE(map_reply.contains("variable:a") || map_reply.contains("variable:b"));

    EXPECT_NO_THROW(msgpack_unpacked = getMsgPackObject(*publisher->sent_messages[1]););
    msgpack_object = msgpack_unpacked.get();
    EXPECT_EQ(msgpack_object.type, msgpack::type::MAP);
    map_reply = msgpack_object.as<Map>();
    EXPECT_EQ(map_reply.contains("error"), true);
    EXPECT_EQ(map_reply.contains(KEY_REPLY_ID), true);
    EXPECT_TRUE(map_reply.contains("variable:a") || map_reply.contains("variable:b"));

    // check for completion message
    EXPECT_NO_THROW(msgpack_unpacked = getMsgPackObject(*publisher->sent_messages[2]););
    msgpack_object = msgpack_unpacked.get();
    EXPECT_EQ(msgpack_object.type, msgpack::type::MAP);
    map_reply = msgpack_object.as<Map>();
    EXPECT_EQ(map_reply.contains("error"), true);
    EXPECT_EQ(map_reply["error"].as<std::int32_t>(), 1);
    EXPECT_EQ(map_reply.contains(KEY_REPLY_ID), true);

    // dispose all
    deinitBackend(std::move(node_controller));
}

TEST(NodeControllerSnapshot, SnapshotCommandWithMonitorMsgPackSer)
{
    typedef std::map<std::string, msgpack::object> Map;
    // wait for two monitor message(event and ack replay) and three snashot (two data and one completion message)
    std::latch                      work_done{5};
    boost::json::object             reply_msg;
    std::unique_ptr<NodeController> node_controller;
    auto                            publisher = std::make_shared<DummyPublisher>(work_done);
    node_controller = initBackend(ncs_tcp_port, publisher, true, true);

    // add the number of reader from topic
    dynamic_cast<ControllerConsumerDummyPublisher*>(publisher.get())->setConsumerNumber(1);
    while (!node_controller->isWorkerReady(CommandType::snapshot) || !node_controller->isWorkerReady(CommandType::multi_monitor))
    {
        sleep(1);
    }

    // start monitor on pva://variable:a that is not going to fire event at specific time so monitor only receive one
    // message
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const MonitorCommand>(MonitorCommand{CommandType::monitor, SerializationType::Msgpack, KAFKA_TOPIC_ACQUIRE_IN, "rep-id-monitor", "pva://variable:a", KAFKA_TOPIC_ACQUIRE_IN})}););

    // wait some time
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // snapshot is going to create a nother monitor watcher on the same pva://variable:a variable and it should work
    // givin a new event, only for that
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const SnapshotCommand>(SnapshotCommand{CommandType::snapshot, SerializationType::Msgpack, KAFKA_TOPIC_ACQUIRE_IN, "rep-id", {"pva://variable:a", "pva://variable:b"}, 1000})}););

    work_done.wait();

    // we need to have publish some message
    size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_EQ(published, 5); // two values and one completion message

    // dispose all
    deinitBackend(std::move(node_controller));
}

TEST(NodeControllerSnapshot, RepeatingSnapshotStartStop)
{
    typedef std::map<std::string, msgpack::object> Map;
    boost::json::object             reply_msg;
    std::unique_ptr<NodeController> node_controller;
       
    auto                            publisher = std::make_shared<TopicCountedTargetPublisher>();
    node_controller = initBackend(ncs_tcp_port, publisher, true, true);

    // add the number of reader from topic
    dynamic_cast<ControllerConsumerDummyPublisher*>(publisher.get())->setConsumerNumber(1);
    while (!node_controller->isWorkerReady(CommandType::repeating_snapshot))
    {
        sleep(1);
    }

    // snapshot is going to create a nother monitor watcher on the same pva://variable:a variable and it should work
    // givin a new event, only for that
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotCommand>(RepeatingSnapshotCommand{
        CommandType::repeating_snapshot,
        SerializationType::Msgpack,
        "app_reply_topic",
        "rep-id",
        "Snapshot Name",
        {"pva://variable:a", "pva://variable:b"},
        0,
        1000,

    })}););

    // wait for activating 1 ack message on app topic and wait for first snapshot 4 (header + 2 data event + completaion) messages
    publisher->wait_for({{"snapshot_name", 4}, {"app_reply_topic", 1}}, std::chrono::milliseconds(60000));

    // we need to have publish some message
    size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_GE(published, 5);

    // stop the snapshot
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotStopCommand>(RepeatingSnapshotStopCommand{
        CommandType::repeating_snapshot_stop,
        SerializationType::Msgpack,
        "app_reply_topic",
        "rep-id",
        "snapshot_name"
    })}););

     while (node_controller->getTaskRunning(CommandType::repeating_snapshot))
    {
        sleep(1);
    }

    // dispose all
    deinitBackend(std::move(node_controller));
}