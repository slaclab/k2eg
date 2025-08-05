
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

#include "../metric/metric.h"
#include "k2eg/controller/command/cmd/MonitorCommand.h"
#include "k2eg/controller/command/cmd/SnapshotCommand.h"
#include "k2eg/service/metric/IMetricService.h"
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
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotCommand>(RepeatingSnapshotCommand{CommandType::repeating_snapshot, SerializationType::Msgpack, "app_reply_topic", "rep-id", "Snapshot Name", {"pva://variable:a", "pva://variable:b"}, 0, 1000, 0, false})}););

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

TEST(NodeControllerSnapshot, RepeatingSnapshtotStartStopVerifyConfiguration)
{
    typedef std::map<std::string, msgpack::object> Map;
    boost::json::object                            reply_msg;
    std::unique_ptr<NodeController>                node_controller;

    auto publisher = std::make_shared<TopicCountedTargetPublisher>();
    node_controller = initBackend(ncs_tcp_port, publisher, true, true);
    auto node_configuration_service = ServiceResolver<INodeConfiguration>::resolve();
    ASSERT_TRUE(node_configuration_service != nullptr);

    node_configuration_service->deleteSnapshotConfiguration("snapshot_name");

    // add the number of reader from topic
    dynamic_cast<ControllerConsumerDummyPublisher*>(publisher.get())->setConsumerNumber(1);
    while (!node_controller->isWorkerReady(CommandType::repeating_snapshot))
    {
        sleep(1);
    }

    // snapshot is going to create a nother monitor watcher on the same pva://variable:a variable and it should work
    // givin a new event, only for that
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotCommand>(RepeatingSnapshotCommand{CommandType::repeating_snapshot, SerializationType::Msgpack, "app_reply_topic", "rep-id", "Snapshot Name", {"pva://variable:a", "pva://variable:b"}, 0, 1000, 0, false})}););

    // // wait for the activation on the ocnfiguration
    bool condition = true;
    int  retry_count = 0;
    while (condition && retry_count < 60)
    {
        retry_count++;
        sleep(1);
        if (node_configuration_service->isSnapshotRunning("snapshot_name"))
        {
            condition = false;
        }
    }

    auto running_snapshots = node_configuration_service->getRunningSnapshots();
    EXPECT_EQ(running_snapshots.size(), 1) << "Running snapshots size is not 1, it is: " << running_snapshots.size();
    ASSERT_NE(std::find(running_snapshots.begin(), running_snapshots.end(), "snapshot_name"), running_snapshots.end()) << "Running snapshot 'snapshot_name' not found in the list of running snapshots";

    auto snapshot_config = node_configuration_service->getSnapshotConfiguration("snapshot_name");
    ASSERT_TRUE(snapshot_config != nullptr);
    EXPECT_EQ(snapshot_config->weight, 0);
    EXPECT_EQ(snapshot_config->weight_unit, "eps");
    EXPECT_STRNE(snapshot_config->update_timestamp.c_str(), "");
    EXPECT_EQ(snapshot_config->config_json, "{\"type\":\"repeating_snapshot\",\"serialization\":\"Msgpack\",\"reply_id\":\"rep-id\",\"reply_topic\":\"app_reply_topic\",\"snapshot_name\":\"Snapshot Name\",\"pv_name_list\":[\"pva://variable:b\",\"pva://variable:a\"],\"repeat_delay_msec\":0,\"time_window_msec\":1000,\"sub_push_delay_msec\":0,\"pv_field_filter_list\":[],\"triggered\":false,\"snapshot_type\":\"Normal\"}");
    // stop the snapshot
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotStopCommand>(RepeatingSnapshotStopCommand{CommandType::repeating_snapshot_stop, SerializationType::Msgpack, "app_reply_topic", "rep-id", "snapshot_name"})}););
    // wait for the stop message succeed
    while (node_controller->getTaskRunning(CommandType::repeating_snapshot))
    {
        sleep(1);
    }

    condition = true;
    retry_count = 0;
    while (condition && retry_count < 60)
    {
        retry_count++;
        sleep(1);
        if (!node_configuration_service->isSnapshotRunning("snapshot_name"))
        {
            condition = false;
        }
    }
    snapshot_config = node_configuration_service->getSnapshotConfiguration("snapshot_name");
    ASSERT_TRUE(snapshot_config != nullptr);
    EXPECT_EQ(node_configuration_service->isSnapshotRunning("snapshot_name"), false);

    // dispose all
    deinitBackend(std::move(node_controller));
}

TEST(NodeControllerSnapshot, RepeatingSnapshotRestartAfterCrash)
{
    typedef std::map<std::string, msgpack::object> Map;
    boost::json::object                            reply_msg;
    std::unique_ptr<NodeController>                node_controller;

    auto publisher = std::make_shared<TopicCountedTargetPublisher>();
    node_controller = initBackend(ncs_tcp_port, publisher, true, true);
    auto node_configuration_service = ServiceResolver<INodeConfiguration>::resolve();
    ASSERT_TRUE(node_configuration_service != nullptr);
    // delete old snapshot configuration if exists
    node_configuration_service->deleteSnapshotConfiguration("snapshot_name");
    // start a new snapshot
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotCommand>(RepeatingSnapshotCommand{CommandType::repeating_snapshot, SerializationType::Msgpack, "app_reply_topic", "rep-id", "Snapshot Name", {"pva://variable:a", "pva://variable:b"}, 0, 1000, 0, false})}););

    // add the number of reader from topic
    dynamic_cast<ControllerConsumerDummyPublisher*>(publisher.get())->setConsumerNumber(1);
    while (!node_controller->isWorkerReady(CommandType::repeating_snapshot))
    {
        sleep(1);
    }

    // wait for the activation on the configuration
    int retry_count = 0;
    while (!node_configuration_service->isSnapshotRunning("snapshot_name") && retry_count < 120)
    {
        retry_count++;
        sleep(1);
    }

    // check if the snapshot is running
    auto running_snapshots = node_configuration_service->getRunningSnapshots();
    EXPECT_EQ(running_snapshots.size(), 1) << "Running snapshots size is not 1, it is: " << running_snapshots.size();
    ASSERT_NE(std::find(running_snapshots.begin(), running_snapshots.end(), "snapshot_name"), running_snapshots.end()) << "Running snapshot 'snapshot_name' not found in the list of running snapshots";

    // we need to simulate the shutdown of the node controller
    deinitBackend(std::move(node_controller));

    // restart the node controller
    publisher = std::make_shared<TopicCountedTargetPublisher>();
    node_controller = initBackend(ncs_tcp_port, publisher, true, true);
    node_configuration_service = ServiceResolver<INodeConfiguration>::resolve();

    // wait until snapshot is restarted
    retry_count = 0;
    while (node_configuration_service->getSnapshotGateway("snapshot_name").empty() && retry_count < 120)
    {
        retry_count++;
        sleep(1);
    }


    auto snapshot_config = node_configuration_service->getSnapshotConfiguration("snapshot_name");
    ASSERT_TRUE(snapshot_config != nullptr);
    EXPECT_EQ(snapshot_config->weight, 0);
    EXPECT_EQ(snapshot_config->weight_unit, "eps");
    EXPECT_STRNE(snapshot_config->update_timestamp.c_str(), "");
    // create json object from snapshot configuration
    boost::json::object snapshot_config_json = boost::json::parse(snapshot_config->config_json).as_object();
    EXPECT_EQ(snapshot_config_json["type"].as_string(), "repeating_snapshot");
    EXPECT_EQ(snapshot_config_json["serialization"].as_string(), "Msgpack");
    EXPECT_EQ(snapshot_config_json["reply_id"].as_string(), "rep-id");
    EXPECT_EQ(snapshot_config_json["reply_topic"].as_string(), "app_reply_topic");
    EXPECT_EQ(snapshot_config_json["snapshot_name"].as_string(), "Snapshot Name");
    EXPECT_EQ(snapshot_config_json["pv_name_list"].as_array().size(), 2);
    auto& pv_list_arr = snapshot_config_json["pv_name_list"].as_array();
    bool has_b = std::any_of(pv_list_arr.begin(), pv_list_arr.end(), [](const boost::json::value& v){
        return v.is_string() && v.as_string() == "pva://variable:b";
    });
    bool has_a = std::any_of(pv_list_arr.begin(), pv_list_arr.end(), [](const boost::json::value& v){
        return v.is_string() && v.as_string() == "pva://variable:a";
    });
    EXPECT_TRUE(has_b);
    EXPECT_TRUE(has_a);
    EXPECT_EQ(snapshot_config_json["repeat_delay_msec"].as_int64(), 0);
    EXPECT_EQ(snapshot_config_json["time_window_msec"].as_int64(), 1000);
    EXPECT_EQ(snapshot_config_json["sub_push_delay_msec"].as_int64(), 0);
    EXPECT_EQ(snapshot_config_json["pv_field_filter_list"].as_array().size(), 0);
    EXPECT_EQ(snapshot_config_json["triggered"].as_bool(), false);
    EXPECT_EQ(snapshot_config_json["snapshot_type"].as_string(), "Normal");

    // stop the snapshot
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotStopCommand>(RepeatingSnapshotStopCommand{CommandType::repeating_snapshot_stop, SerializationType::Msgpack, "app_reply_topic", "rep-id", "snapshot_name"})}););
    // wait for the stop message succeed
    while (node_controller->getTaskRunning(CommandType::repeating_snapshot))
    {
        sleep(1);
    }

    retry_count = 0;
    while (node_configuration_service->isSnapshotRunning("snapshot_name") && retry_count < 120)
    {
        retry_count++;
        sleep(1);
    }
    snapshot_config = node_configuration_service->getSnapshotConfiguration("snapshot_name");
    ASSERT_TRUE(snapshot_config != nullptr);
    EXPECT_EQ(node_configuration_service->isSnapshotRunning("snapshot_name"), false);

    // dispose all
    deinitBackend(std::move(node_controller));
}

TEST(NodeControllerSnapshot, RepeatingSnapshotStoppedSnapshotsAreNotRestarted)
{
    typedef std::map<std::string, msgpack::object> Map;
    boost::json::object                            reply_msg;
    std::unique_ptr<NodeController>                node_controller;

    auto publisher = std::make_shared<TopicCountedTargetPublisher>();
    node_controller = initBackend(ncs_tcp_port, publisher, true, true);
    auto node_configuration_service = ServiceResolver<INodeConfiguration>::resolve();
    ASSERT_TRUE(node_configuration_service != nullptr);
    // delete old snapshot configuration if exists
    node_configuration_service->deleteSnapshotConfiguration("snapshot_name");
    // start a new snapshot
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotCommand>(RepeatingSnapshotCommand{CommandType::repeating_snapshot, SerializationType::Msgpack, "app_reply_topic", "rep-id", "Snapshot Name", {"pva://variable:a", "pva://variable:b"}, 0, 1000, 0, false})}););

    // add the number of reader from topic
    dynamic_cast<ControllerConsumerDummyPublisher*>(publisher.get())->setConsumerNumber(1);
    while (!node_controller->isWorkerReady(CommandType::repeating_snapshot))
    {
        sleep(1);
    }

    // wait for the activation on the configuration
    int retry_count = 0;
    while (!node_configuration_service->isSnapshotRunning("snapshot_name") && retry_count < 120)
    {
        retry_count++;
        sleep(1);
    }
    // stop the snapshot
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotStopCommand>(RepeatingSnapshotStopCommand{CommandType::repeating_snapshot_stop, SerializationType::Msgpack, "app_reply_topic", "rep-id", "snapshot_name"})}););
    // wait for the stop message succeed
    while (node_controller->getTaskRunning(CommandType::repeating_snapshot))
    {
        sleep(1);
    }

    retry_count = 0;
    while (node_configuration_service->isSnapshotRunning("snapshot_name") && retry_count < 120)
    {
        retry_count++;
        sleep(1);
    }

    // dispose all
    deinitBackend(std::move(node_controller));

    // restart the node controller
    publisher = std::make_shared<TopicCountedTargetPublisher>();
    node_controller = initBackend(ncs_tcp_port, publisher, true, true);

    retry_count = 0;
    while (!node_configuration_service->isSnapshotRunning("snapshot_name") && retry_count < 60)
    {
        retry_count++;
        sleep(1);
    }
    // snapshot should not be running now
    EXPECT_FALSE(node_configuration_service->isSnapshotRunning("snapshot_name"));

}

TEST(NodeControllerSnapshot, RepeatingSnapshotStartStopTwice)
{
    typedef std::map<std::string, msgpack::object> Map;
    boost::json::object                            reply_msg;
    std::unique_ptr<NodeController>                node_controller;

    auto publisher = std::make_shared<TopicCountedTargetPublisher>();
    node_controller = initBackend(ncs_tcp_port, publisher, true, true);

    // add the number of reader from topic
    dynamic_cast<ControllerConsumerDummyPublisher*>(publisher.get())->setConsumerNumber(1);
    while (!node_controller->isWorkerReady(CommandType::repeating_snapshot))
    {
        sleep(1);
    }

    // snapshot is going to create a nother monitor watcher on the same pva://variable:a variable and it should work
    // givin a new event, only for that
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotCommand>(RepeatingSnapshotCommand{CommandType::repeating_snapshot, SerializationType::Msgpack, "app_reply_topic", "rep-id", "Snapshot Name", {"pva://variable:a", "pva://variable:b"}, 0, 1000, 0, false})}););

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
    // wait for ack command
    sleep(5);

    // redo the test again
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotCommand>(RepeatingSnapshotCommand{CommandType::repeating_snapshot, SerializationType::Msgpack, "app_reply_topic", "rep-id", "Snapshot Name", {"pva://variable:a", "pva://variable:b"}, 0, 1000, 0, false})}););

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
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotCommand>(RepeatingSnapshotCommand{CommandType::repeating_snapshot, SerializationType::Msgpack, "app_reply_topic", "rep-id", "Snapshot Name", {"pva://variable:a", "pva://variable:b"}, 0, 1000, 0, true})}););

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
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotTriggerCommand>(RepeatingSnapshotTriggerCommand{CommandType::repeating_snapshot_trigger, SerializationType::Msgpack, "app_reply_topic", "rep-id", "snapshot_name"})}););

    auto topic_counts_trigger = publisher->wait_for({{"snapshot_name", 4}, {"app_reply_topic", 1}}, std::chrono::milliseconds(60000));
    EXPECT_EQ(topic_counts_trigger["snapshot_name"], 4);
    EXPECT_EQ(topic_counts_trigger["app_reply_topic"], 1);

    // fetch reply message
    EXPECT_NO_THROW(msgpack_unpacked = exstractMsgpackObjectAtIndex(publisher->sent_messages, "app_reply_topic", 1, true););
    msgpack_object = msgpack_unpacked.get();
    EXPECT_EQ(getMSGPackObjectForKey(msgpack_object, "error").as<int>(), 0);

    // fetch data header
    int64_t data_ts = 0;
    int     iter_index = 0;
    EXPECT_NO_THROW(msgpack_unpacked = exstractMsgpackObjectAtIndex(publisher->sent_messages, "snapshot_name", 0, true););
    msgpack_object = msgpack_unpacked.get();
    EXPECT_EQ(getMSGPackObjectForKey(msgpack_object, "message_type").as<int>(), 0);
    EXPECT_STREQ(getMSGPackObjectForKey(msgpack_object, "snapshot_name").as<std::string>().c_str(), "Snapshot Name");
    EXPECT_GT((data_ts = getMSGPackObjectForKey(msgpack_object, "timestamp").as<std::int64_t>()), 0);
    EXPECT_GT((iter_index = getMSGPackObjectForKey(msgpack_object, "iter_index").as<int64_t>()), 0);

    // fetch data event
    EXPECT_NO_THROW(msgpack_unpacked = exstractMsgpackObjectAtIndex(publisher->sent_messages, "snapshot_name", 1, true););
    msgpack_object = msgpack_unpacked.get();
    EXPECT_EQ(getMSGPackObjectForKey(msgpack_object, "message_type").as<int>(), 1);
    EXPECT_EQ(getMSGPackObjectForKey(msgpack_object, "timestamp").as<int64_t>(), data_ts);
    EXPECT_EQ(getMSGPackObjectForKey(msgpack_object, "iter_index").as<int64_t>(), iter_index);
    EXPECT_TRUE(checkMSGPackObjectContains(msgpack_object, "variable:a") || checkMSGPackObjectContains(msgpack_object, "variable:b"));

    EXPECT_NO_THROW(msgpack_unpacked = exstractMsgpackObjectAtIndex(publisher->sent_messages, "snapshot_name", 2, true););
    msgpack_object = msgpack_unpacked.get();
    EXPECT_EQ(getMSGPackObjectForKey(msgpack_object, "message_type").as<int>(), 1);
    EXPECT_EQ(getMSGPackObjectForKey(msgpack_object, "timestamp").as<int64_t>(), data_ts);
    EXPECT_EQ(getMSGPackObjectForKey(msgpack_object, "iter_index").as<int64_t>(), iter_index);
    EXPECT_TRUE(checkMSGPackObjectContains(msgpack_object, "variable:a") || checkMSGPackObjectContains(msgpack_object, "variable:b"));

    // fetch data tail event
    EXPECT_NO_THROW(msgpack_unpacked = exstractMsgpackObjectAtIndex(publisher->sent_messages, "snapshot_name", 3, true););
    msgpack_object = msgpack_unpacked.get();
    EXPECT_EQ(getMSGPackObjectForKey(msgpack_object, "message_type").as<int>(), 2);
    EXPECT_EQ(getMSGPackObjectForKey(msgpack_object, "timestamp").as<int64_t>(), data_ts);
    EXPECT_EQ(getMSGPackObjectForKey(msgpack_object, "iter_index").as<int64_t>(), iter_index);
    EXPECT_EQ(getMSGPackObjectForKey(msgpack_object, "error").as<int>(), 0);
    EXPECT_STREQ(getMSGPackObjectForKey(msgpack_object, "snapshot_name").as<std::string>().c_str(), "Snapshot Name");

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

TEST(NodeControllerSnapshot, RepeatingSnapshotTimeBufferedType)
{
    typedef std::map<std::string, msgpack::object> Map;
    boost::json::object                            reply_msg;
    std::unique_ptr<NodeController>                node_controller;

    auto publisher = std::make_shared<TopicCountedTargetPublisher>();
    node_controller = initBackend(ncs_tcp_port, publisher, true, true);

    // add the number of reader from topic
    dynamic_cast<ControllerConsumerDummyPublisher*>(publisher.get())->setConsumerNumber(1);
    while (!node_controller->isWorkerReady(CommandType::repeating_snapshot))
    {
        sleep(1);
    }

    // snapshot is going to create a nother monitor watcher on the same pva://variable:a variable and it should work
    // givin a new event, only for that
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotCommand>(RepeatingSnapshotCommand{CommandType::repeating_snapshot, SerializationType::Msgpack, "app_reply_topic", "rep-id", "Snapshot Name", {"pva://variable:a", "pva://channel:ramp:ramp"}, 0, 4000, 0, false, SnapshotType::TIMED_BUFFERED})}););

    // wait for activating 1 ack message on app topic and wait for first snapshot 4 (header + 2 data event +
    // completaion) messages
    auto topic_counts = publisher->wait_for({{"snapshot_name", 5}, {"app_reply_topic", 1}}, std::chrono::milliseconds(10000));

    // we need to have publish some message
    size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_GE(topic_counts["snapshot_name"], 5);
    EXPECT_EQ(topic_counts["app_reply_topic"], 1);

    // check all messages
    for (int idx = 0; idx < publisher->sent_messages.size(); idx++)
    {
        typedef std::map<std::string, msgpack::object> Map;
        msgpack::unpacked                              msgpack_unpacked;
        EXPECT_NO_THROW(msgpack_unpacked = exstractMsgpackObjectAtIndex(publisher->sent_messages, "snapshot_name", idx, false););
        auto msgpack_object = msgpack_unpacked.get();
        std::cout << msgpack_object << std::endl;
    }

    // stop the snapshot
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotStopCommand>(RepeatingSnapshotStopCommand{CommandType::repeating_snapshot_stop, SerializationType::Msgpack, "app_reply_topic", "rep-id", "snapshot_name"})}););
    // wait for ack command
    sleep(1);

    // wait for the stop message succeed
    while (node_controller->getTaskRunning(CommandType::repeating_snapshot))
    {
        sleep(1);
    }

    // dispose all
    deinitBackend(std::move(node_controller));
}

TEST(NodeControllerSnapshot, RepeatingSnapshotTimeBufferedTypeFilteringFields)
{
    typedef std::map<std::string, msgpack::object> Map;
    boost::json::object                            reply_msg;
    std::unique_ptr<NodeController>                node_controller;

    auto publisher = std::make_shared<TopicCountedTargetPublisher>();
    node_controller = initBackend(ncs_tcp_port, publisher, true, true);

    // add the number of reader from topic
    dynamic_cast<ControllerConsumerDummyPublisher*>(publisher.get())->setConsumerNumber(1);
    while (!node_controller->isWorkerReady(CommandType::repeating_snapshot))
    {
        sleep(1);
    }

    // snapshot is going to create a nother monitor watcher on the same pva://variable:a variable and it should work
    // givin a new event, only for that
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotCommand>(RepeatingSnapshotCommand{CommandType::repeating_snapshot, SerializationType::Msgpack, "app_reply_topic", "rep-id", "Snapshot Name", {"pva://variable:a", "pva://channel:ramp:ramp"}, 0, 4000, 0, false, SnapshotType::TIMED_BUFFERED, {"value"}})}););

    // wait for activating 1 ack message on app topic and wait for first snapshot 4 (header + 2 data event +
    // completaion) messages
    auto topic_counts = publisher->wait_for({{"snapshot_name", 5}, {"app_reply_topic", 1}}, std::chrono::milliseconds(10000));

    // we need to have publish some message
    size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_GE(topic_counts["snapshot_name"], 5);
    EXPECT_EQ(topic_counts["app_reply_topic"], 1);

    // check all messages
    for (int idx = 0; idx < publisher->sent_messages.size(); idx++)
    {
        typedef std::map<std::string, msgpack::object> Map;
        msgpack::unpacked                              msgpack_unpacked;
        EXPECT_NO_THROW(msgpack_unpacked = exstractMsgpackObjectAtIndex(publisher->sent_messages, "snapshot_name", idx, false););
        auto msgpack_object = msgpack_unpacked.get();
        std::cout << msgpack_object << std::endl;
    }

    // stop the snapshot
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotStopCommand>(RepeatingSnapshotStopCommand{CommandType::repeating_snapshot_stop, SerializationType::Msgpack, "app_reply_topic", "rep-id", "snapshot_name"})}););
    // wait for ack command
    sleep(1);

    // wait for the stop message succeed
    while (node_controller->getTaskRunning(CommandType::repeating_snapshot))
    {
        sleep(1);
    }

    // dispose all
    deinitBackend(std::move(node_controller));
}

TEST(NodeControllerSnapshot, RepeatingSnapshotTimeBufferedTypeFilteringFieldsWithManualTrigger)
{
    std::vector<int>                               checked_values;
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
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotCommand>(RepeatingSnapshotCommand{CommandType::repeating_snapshot, SerializationType::Msgpack, "app_reply_topic", "rep-id", "Snapshot Name", {"pva://variable:a", "pva://channel:ramp:ramp"}, 0, 4000, 0, true, SnapshotType::TIMED_BUFFERED, {"value"}})}););

    // wait only ack
    auto topic_counts = publisher->wait_for({{"app_reply_topic", 1}}, std::chrono::milliseconds(10000));

    // we need to have publish some message
    size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_EQ(topic_counts["app_reply_topic"], 1);

    // wait 5 seconds to let the snapshot start and acquire data overflowing the timewindow
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    // trigger the snapshot
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotTriggerCommand>(RepeatingSnapshotTriggerCommand{CommandType::repeating_snapshot_trigger, SerializationType::Msgpack, "app_reply_topic", "rep-id", "snapshot_name"})}););
    topic_counts = publisher->wait_for({{"snapshot_name", 6}}, std::chrono::milliseconds(10000));

    EXPECT_GE(topic_counts["snapshot_name"], 6);
    // check all messages
    for (int idx = 0; idx < publisher->sent_messages.size(); idx++)
    {
        if (publisher->sent_messages[idx] == nullptr)
        {
            continue;
        }
        typedef std::map<std::string, msgpack::object> Map;
        msgpack::unpacked                              msgpack_unpacked;
        EXPECT_NO_THROW(msgpack_unpacked = exstractMsgpackObjectAtIndex(publisher->sent_messages, "snapshot_name", idx, false););
        auto msgpack_object = msgpack_unpacked.get();
        if (msgpack_object.type == msgpack::type::NIL)
        {
            continue;
        }
        std::cout << msgpack_object << std::endl;
        if (checkMSGPackObjectContains(msgpack_object, "channel:ramp:ramp"))
        {
            auto channel_obj = getMSGPackObjectForKey(msgpack_object, "channel:ramp:ramp");
            int  value = static_cast<int>(getMSGPackObjectForKey(channel_obj, "value").as<float>());
            checked_values.push_back(value);
        }
    }
    // check the sequence of the values
    for (size_t i = 1; i < checked_values.size(); ++i)
    {
        EXPECT_TRUE(checked_values[i] == checked_values[i - 1] + 1 || (checked_values[i - 1] == 10 && checked_values[i] == 0));
    }

    // clear all messages
    publisher->sent_messages.clear();
    // wait for 5 seconds to let the snapshot start and acquire data overflowing the timewindow
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    // trigger the snapshot again
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotTriggerCommand>(RepeatingSnapshotTriggerCommand{CommandType::repeating_snapshot_trigger, SerializationType::Msgpack, "app_reply_topic", "rep-id", "snapshot_name"})}););
    topic_counts = publisher->wait_for({{"snapshot_name", 6}}, std::chrono::milliseconds(10000));

    EXPECT_GE(topic_counts["snapshot_name"], 6);
    // check all messages
    checked_values.clear();
    for (int idx = 0; idx < publisher->sent_messages.size(); idx++)
    {
        if (publisher->sent_messages[idx] == nullptr)
        {
            continue;
        }
        typedef std::map<std::string, msgpack::object> Map;
        msgpack::unpacked                              msgpack_unpacked;
        EXPECT_NO_THROW(msgpack_unpacked = exstractMsgpackObjectAtIndex(publisher->sent_messages, "snapshot_name", idx, false););
        auto msgpack_object = msgpack_unpacked.get();
        if (msgpack_object.type == msgpack::type::NIL)
        {
            continue;
        }
        std::cout << msgpack_object << std::endl;
        if (checkMSGPackObjectContains(msgpack_object, "channel:ramp:ramp"))
        {
            auto channel_obj = getMSGPackObjectForKey(msgpack_object, "channel:ramp:ramp");
            int  value = static_cast<int>(getMSGPackObjectForKey(channel_obj, "value").as<float>());
            checked_values.push_back(value);
        }
    }

    // check the sequence of the values
    for (size_t i = 1; i < checked_values.size(); ++i)
    {
        EXPECT_TRUE(checked_values[i] == checked_values[i - 1] + 1 || (checked_values[i - 1] == 10 && checked_values[i] == 0));
    }

    // stop the snapshot
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotStopCommand>(RepeatingSnapshotStopCommand{CommandType::repeating_snapshot_stop, SerializationType::Msgpack, "app_reply_topic", "rep-id", "snapshot_name"})}););
    // wait for ack command
    sleep(1);

    // wait for the stop message succeed
    while (node_controller->getTaskRunning(CommandType::repeating_snapshot))
    {
        sleep(1);
    }

    // dispose all
    deinitBackend(std::move(node_controller));
}

#include <malloc.h>

TEST(NodeControllerSnapshot, RepeatingSnapshotTimeBufferedTypeFilteringFieldsHighRate)
{
    typedef std::map<std::string, msgpack::object> Map;
    boost::json::object                            reply_msg;
    std::unique_ptr<NodeController>                node_controller;

    auto publisher = std::make_shared<DischargePublisher>();
    node_controller = initBackend(ncs_tcp_port, publisher, false, true);

    // add the number of reader from topic
    dynamic_cast<DischargePublisher*>(publisher.get())->setConsumerNumber(1);
    while (!node_controller->isWorkerReady(CommandType::repeating_snapshot))
    {
        sleep(1);
        // print the statistics getting from http port
    }

    // snapshot is going to create a nother monitor watcher on the same pva://variable:a variable and it should work
    // givin a new event, only for that
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotCommand>(RepeatingSnapshotCommand{CommandType::repeating_snapshot, SerializationType::Msgpack, "app_reply_topic", "rep-id", "Snapshot Name", {"pva://variable:a", "pva://channel:random:fast"}, 0, 4000, 0, false, SnapshotType::TIMED_BUFFERED, {"value"}})}););

    // wait 60 seconds to let the snapshot start and acquire data overflowing the timewindow
    //  get now
    auto& system_metrics = ServiceResolver<IMetricService>::resolve()->getNodeControllerSystemMetric();
    auto  start_time = std::chrono::steady_clock::now();
    int   idx = 0;
    while (true)
    {
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed > std::chrono::seconds(60))
        {
            break;
        }
        // print statisdtics
        auto metrics_string = getUrl(METRIC_URL_FROM_PORT(ncs_tcp_port));
        printSystemMetricsTable(metrics_string, idx++ == 0);
        sleep(1);
    }

    // stop the snapshot
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotStopCommand>(RepeatingSnapshotStopCommand{CommandType::repeating_snapshot_stop, SerializationType::Msgpack, "app_reply_topic", "rep-id", "snapshot_name"})}););
    // wait for ack command
    sleep(1);

    // wait for the stop message succeed
    while (node_controller->getTaskRunning(CommandType::repeating_snapshot))
    {
        sleep(1);
    }

    // restart the snapshto
    // givin a new event, only for that
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotCommand>(RepeatingSnapshotCommand{CommandType::repeating_snapshot, SerializationType::Msgpack, "app_reply_topic", "rep-id", "Snapshot Name", {"pva://variable:a", "pva://channel:random:fast"}, 0, 4000, 0, false, SnapshotType::TIMED_BUFFERED, {"value"}})}););
    start_time = std::chrono::steady_clock::now();
    while (true)
    {
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed > std::chrono::seconds(60))
        {
            break;
        }
        // print statisdtics
        auto metrics_string = getUrl(METRIC_URL_FROM_PORT(ncs_tcp_port));
        printSystemMetricsTable(metrics_string, idx++ == 0);
        sleep(1);
    }

    // stop the snapshot
    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotStopCommand>(RepeatingSnapshotStopCommand{CommandType::repeating_snapshot_stop, SerializationType::Msgpack, "app_reply_topic", "rep-id", "snapshot_name"})}););
    while (node_controller->getTaskRunning(CommandType::repeating_snapshot))
    {
        sleep(1);
    }
    malloc_trim(0);
    sleep(5);
    // fetch metric at the end
    auto metrics_string = getUrl(METRIC_URL_FROM_PORT(ncs_tcp_port));
    printSystemMetricsTable(metrics_string, false);

    // dispose all
    deinitBackend(std::move(node_controller));
}

TEST(NodeControllerSnapshot, RepeatingSnapshotTimeBufferedTypeStartAndStopLoop)
{
    typedef std::map<std::string, msgpack::object> Map;
    boost::json::object                            reply_msg;
    std::unique_ptr<NodeController>                node_controller;

    auto publisher = std::make_shared<DischargePublisher>();
    node_controller = initBackend(ncs_tcp_port, publisher, false, true);

    // add the number of reader from topic
    dynamic_cast<DischargePublisher*>(publisher.get())->setConsumerNumber(1);
    while (!node_controller->isWorkerReady(CommandType::repeating_snapshot))
    {
        sleep(1);
        // print the statistics getting from http port
    }

    auto& system_metrics = ServiceResolver<IMetricService>::resolve()->getNodeControllerSystemMetric();
    auto  start_time = std::chrono::steady_clock::now();
    int   idx = 0;
    while (true)
    {
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed > std::chrono::seconds(60))
        {
            break;
        }

        EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotCommand>(
            RepeatingSnapshotCommand{
                CommandType::repeating_snapshot,
                SerializationType::Msgpack,
                "app_reply_topic",
                "rep-id",
                "Snapshot Name",
                {"pva://variable:a", "pva://channel:random:fast"},
                0,
                1000,
                0,
                false,
                SnapshotType::TIMED_BUFFERED,
                {"value"}})}););

        sleep(2);
        // print statisdtics
        auto metrics_string = getUrl(METRIC_URL_FROM_PORT(ncs_tcp_port));
        printSystemMetricsTable(metrics_string, idx++ == 0);
        // stop the snapshot
        EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const RepeatingSnapshotStopCommand>(RepeatingSnapshotStopCommand{CommandType::repeating_snapshot_stop, SerializationType::Msgpack, "app_reply_topic", "rep-id", "snapshot_name"})}););
        // wait for ack command
        sleep(1);
    }
    sleep(5);
    // fetch metric at the end
    auto metrics_string = getUrl(METRIC_URL_FROM_PORT(ncs_tcp_port));
    printSystemMetricsTable(metrics_string, false);

    // dispose all
    deinitBackend(std::move(node_controller));
}