
#include "NodeControllerCommon.h"

#include <gtest/gtest.h>
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
    node_controller = initBackend(ncs_tcp_port, publisher, false, true);

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
    boost::json::object                            reply_msg;
    std::unique_ptr<NodeController>                node_controller;

    auto publisher = std::make_shared<TopicCountedTargetPublisher>();
    node_controller = initBackend(ncs_tcp_port, publisher, false, true);

    // add the number of reader from topic
    dynamic_cast<ControllerConsumerDummyPublisher*>(publisher.get())->setConsumerNumber(1);
    while (!node_controller->isWorkerReady(CommandType::repeating_snapshot))
    {
        sleep(1);
    }

    // snapshot is going to create a nother monitor watcher on the same pva://variable:a variable and it should work
    // givin a new event, only for that
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotCommand>(RepeatingSnapshotCommand{CommandType::repeating_snapshot, SerializationType::Msgpack, "app_reply_topic", "rep-id", "Snapshot Name", {"pva://variable:a", "pva://variable:b"}, 0, 1000, false})}););

    // wait for activating 1 ack message on app topic and wait for first snapshot 4 (header + 2 data event +
    // completaion) messages
    auto topic_counts = publisher->wait_for({{"snapshot_name", 4}, {"app_reply_topic", 1}}, std::chrono::milliseconds(10000));

    // we need to have publish some message
    size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_EQ(published, 5);
    EXPECT_EQ(topic_counts["snapshot_name"], 4);
    EXPECT_EQ(topic_counts["app_reply_topic"], 1);

    // stop the snapshot
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotStopCommand>(RepeatingSnapshotStopCommand{CommandType::repeating_snapshot_stop, SerializationType::Msgpack, "app_reply_topic", "rep-id", "snapshot_name"})}););
    // wait for the stop message succeed
    while (node_controller->getTaskRunning(CommandType::repeating_snapshot))
    {
        sleep(1);
    }

    // dispose all
    deinitBackend(std::move(node_controller));
}

TEST(NodeControllerSnapshot, RepeatingSnapshotStartStopTwice)
{
    typedef std::map<std::string, msgpack::object> Map;
    boost::json::object                            reply_msg;
    std::unique_ptr<NodeController>                node_controller;

    auto publisher = std::make_shared<TopicCountedTargetPublisher>();
    node_controller = initBackend(ncs_tcp_port, publisher, false, true);

    // add the number of reader from topic
    dynamic_cast<ControllerConsumerDummyPublisher*>(publisher.get())->setConsumerNumber(1);
    while (!node_controller->isWorkerReady(CommandType::repeating_snapshot))
    {
        sleep(1);
    }

    // snapshot is going to create a nother monitor watcher on the same pva://variable:a variable and it should work
    // givin a new event, only for that
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotCommand>(RepeatingSnapshotCommand{CommandType::repeating_snapshot, SerializationType::Msgpack, "app_reply_topic", "rep-id", "Snapshot Name", {"pva://variable:a", "pva://variable:b"}, 0, 1000, false})}););

    // wait for activating 1 ack message on app topic and wait for first snapshot 4 (header + 2 data event +
    // completaion) messages
    auto topic_counts = publisher->wait_for({{"snapshot_name", 4}, {"app_reply_topic", 1}}, std::chrono::milliseconds(10000));

    // we need to have publish some message
    size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_EQ(published, 5);
    EXPECT_EQ(topic_counts["snapshot_name"], 4);
    EXPECT_EQ(topic_counts["app_reply_topic"], 1);

    // stop the snapshot
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotStopCommand>(RepeatingSnapshotStopCommand{CommandType::repeating_snapshot_stop, SerializationType::Msgpack, "app_reply_topic", "rep-id", "snapshot_name"})}););
    
    sleep(1);

    //redo the test again
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotCommand>(
        RepeatingSnapshotCommand{
            CommandType::repeating_snapshot, 
            SerializationType::Msgpack, 
            "app_reply_topic", "rep-id",
            "Snapshot Name",
            {"pva://variable:a", "pva://variable:b"},
            0, 
            1000, 
            false})}););

    // wait for activating 1 ack message on app topic and wait for first snapshot 4 (header + 2 data event +
    // completaion) messages
    topic_counts = publisher->wait_for({{"snapshot_name", 4}, {"app_reply_topic", 1}}, std::chrono::milliseconds(10000));

    // we need to have publish some message
    EXPECT_EQ(topic_counts["snapshot_name"], 4);
    EXPECT_EQ(topic_counts["app_reply_topic"], 1);

    // stop the snapshot
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotStopCommand>(RepeatingSnapshotStopCommand{CommandType::repeating_snapshot_stop, SerializationType::Msgpack, "app_reply_topic", "rep-id", "snapshot_name"})}););

    
    // wait for the stop message succeed
    while (node_controller->getTaskRunning(CommandType::repeating_snapshot))
    {
        sleep(1);
    }

    // dispose all
    deinitBackend(std::move(node_controller));
}

TEST(NodeControllerSnapshot, RepeatingTriggeredSnapshotStartTriggerStop)
{
    typedef std::map<std::string, msgpack::object> Map;
    msgpack::unpacked                              msgpack_unpacked;

    boost::json::object             reply_msg;
    std::unique_ptr<NodeController> node_controller;

    auto publisher = std::make_shared<TopicCountedTargetPublisher>();
    node_controller = initBackend(ncs_tcp_port, publisher, false, true);

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
        true
    })}););

    // try to listen on snapshot_name but it will not redceive anything so the wait will exipres on the specific timeout
    auto topic_counts = publisher->wait_for({{"snapshot_name", 4}, {"app_reply_topic", 1}}, std::chrono::milliseconds(4000));

    // we need to have received only the ack for the snapshto submission
    EXPECT_EQ(topic_counts["snapshot_name"], 0);
    EXPECT_EQ(topic_counts["app_reply_topic"], 1);

    // check for data validation

    EXPECT_NO_THROW(msgpack_unpacked = exstractMsgpackObjectAtIndex(publisher->sent_messages, "app_reply_topic", 0, true););
    auto msgpack_object = msgpack_unpacked.get();
    EXPECT_EQ(getMSGPackObjectForKey(msgpack_object, "error").as<int>(), 0);
    EXPECT_STREQ(getMSGPackObjectForKey(msgpack_object, KEY_REPLY_ID).as<std::string>().c_str(), "rep-id");
    EXPECT_STREQ(getMSGPackObjectForKey(msgpack_object, "publishing_topic").as<std::string>().c_str(), "snapshot_name");

    // now perform the trigger to let receive the snapshto values
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotTriggerCommand>(RepeatingSnapshotTriggerCommand{
        CommandType::repeating_snapshot_trigger, 
        SerializationType::Msgpack, 
        "app_reply_topic", 
        "rep-id", 
        "snapshot_name"
    })}););

    auto topic_counts_trigger = publisher->wait_for({{"snapshot_name", 4}, {"app_reply_topic", 1}}, std::chrono::milliseconds(60000));
    EXPECT_EQ(topic_counts_trigger["snapshot_name"], 4);
    EXPECT_EQ(topic_counts_trigger["app_reply_topic"], 1);

    // fetch reply message
    EXPECT_NO_THROW(msgpack_unpacked = exstractMsgpackObjectAtIndex(publisher->sent_messages, "app_reply_topic", 1, true););
    msgpack_object = msgpack_unpacked.get();
    EXPECT_EQ(getMSGPackObjectForKey(msgpack_object, "error").as<int>(), 0);

    // fetch data header
    int64_t data_ts = 0;
    int iter_index = 0;
    EXPECT_NO_THROW(msgpack_unpacked = exstractMsgpackObjectAtIndex(publisher->sent_messages, "snapshot_name", 0, true););
    msgpack_object = msgpack_unpacked.get();
    EXPECT_EQ(getMSGPackObjectForKey(msgpack_object, "message_type").as<int>(), 0);
    EXPECT_STREQ(getMSGPackObjectForKey(msgpack_object, "snapshot_name").as<std::string>().c_str(), "Snapshot Name");
    EXPECT_GT((data_ts = getMSGPackObjectForKey(msgpack_object, "timestamp").as<int>()), 0);
    EXPECT_GT((iter_index = getMSGPackObjectForKey(msgpack_object, "iter_index").as<int>()), 0);

    // fetch data event
    EXPECT_NO_THROW(msgpack_unpacked = exstractMsgpackObjectAtIndex(publisher->sent_messages, "snapshot_name", 1, true););
    msgpack_object = msgpack_unpacked.get();
    EXPECT_EQ(getMSGPackObjectForKey(msgpack_object, "message_type").as<int>(), 1);
    EXPECT_EQ(getMSGPackObjectForKey(msgpack_object, "timestamp").as<int>(), data_ts);
    EXPECT_EQ(getMSGPackObjectForKey(msgpack_object, "iter_index").as<int>(), iter_index);
    EXPECT_TRUE(checkMSGPackObjectContains(msgpack_object, "variable:a") || checkMSGPackObjectContains(msgpack_object, "variable:b"));

    EXPECT_NO_THROW(msgpack_unpacked = exstractMsgpackObjectAtIndex(publisher->sent_messages, "snapshot_name", 2, true););
    msgpack_object = msgpack_unpacked.get();
    EXPECT_EQ(getMSGPackObjectForKey(msgpack_object, "message_type").as<int>(), 1);
    EXPECT_EQ(getMSGPackObjectForKey(msgpack_object, "timestamp").as<int>(), data_ts);
    EXPECT_EQ(getMSGPackObjectForKey(msgpack_object, "iter_index").as<int>(), iter_index);
    EXPECT_TRUE(checkMSGPackObjectContains(msgpack_object, "variable:a") || checkMSGPackObjectContains(msgpack_object, "variable:b"));

    // fetch data tail event
    EXPECT_NO_THROW(msgpack_unpacked = exstractMsgpackObjectAtIndex(publisher->sent_messages, "snapshot_name", 3, true););
    msgpack_object = msgpack_unpacked.get();
    EXPECT_EQ(getMSGPackObjectForKey(msgpack_object, "message_type").as<int>(), 2);
    EXPECT_EQ(getMSGPackObjectForKey(msgpack_object, "timestamp").as<int>(), data_ts);
    EXPECT_EQ(getMSGPackObjectForKey(msgpack_object, "iter_index").as<int>(), iter_index);
    EXPECT_EQ(getMSGPackObjectForKey(msgpack_object, "error").as<int>(), 0);
    EXPECT_STREQ(getMSGPackObjectForKey(msgpack_object, "snapshot_name").as<std::string>().c_str(), "Snapshot Name");

    // stop the snapshot
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotStopCommand>(RepeatingSnapshotStopCommand{
        CommandType::repeating_snapshot_stop, 
        SerializationType::Msgpack, 
        "app_reply_topic", 
        "rep-id", 
        "snapshot_name"
    })}););
    // wait for the stop message succeed
    while (node_controller->getTaskRunning(CommandType::repeating_snapshot))
    {
        sleep(1);
    }

    // dispose all
    deinitBackend(std::move(node_controller));
}