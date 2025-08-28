#include "../NodeUtilities.h"
#include "k2eg/common/BaseSerialization.h"
#include "k2eg/controller/command/cmd/SnapshotCommand.h"
#include "k2eg/controller/node/NodeController.h"
#include "gtest/gtest.h"

int k2eg_controller_storage_snapshot_test_port = 20600;

using namespace k2eg::common;
using namespace k2eg::service;;
using namespace k2eg::service::pubsub;
using namespace k2eg::service::storage;
using namespace k2eg::controller::node;
using namespace k2eg::controller::command::cmd;

#define REPLY_TOPIC "app_reply_topic"
#define SNAPSHOT_NAME "snapshot_name"

TEST(NodeControllerStorageSnapshotTest, StartRecording)
{
    SubscriberInterfaceElementVector received_msg;
    auto                             k2eg = startK2EG(
        k2eg_controller_storage_snapshot_test_port,
        NodeType::FULL,
        true,
        true);
    ASSERT_NE(k2eg, nullptr) << "Failed to create K2EG instance";
    ASSERT_TRUE(k2eg->isRunning()) << "K2EG instance is not started";

    sleep(2);

    auto& node_controller = k2eg->getNodeControllerReference();

    auto publisher = k2eg->getPublisherInstance();
    ASSERT_NE(publisher, nullptr) << "Failed to get publisher instance";

    auto subscriber_reply = k2eg->getSubscriberInstance(REPLY_TOPIC);
    ASSERT_NE(subscriber_reply, nullptr) << "Failed to get subscriber instance for reply";

    // auto subscriber_snapshot = k2eg->getSubscriberInstance(SNAPSHOT_NAME);
    // ASSERT_NE(subscriber_snapshot, nullptr) << "Failed to get subscriber instance for snapshot";

    auto storage_service = k2eg->getStorageServiceInstance();
    ASSERT_NE(storage_service, nullptr) << "Failed to get storage service instance";

    // remove all data
    storage_service->clearAllData();

    // start a snapshot
    auto start_snapshot_cmd = MakeRepeatingSnapshotCommandShrdPtr(
        SerializationType::Msgpack,
        REPLY_TOPIC,
        "rep-id",
        SNAPSHOT_NAME,
        std::unordered_set<std::string>{"pva://variable:a", "pva://variable:b"},
        0,
        1000,
        0,
        false,
        SnapshotType::NORMAL,
        std::unordered_set<std::string>{});

    // send snapshot request
    k2eg->sendCommand(publisher, std::make_unique<CMDMessage<RepeatingSnapshotCommandShrdPtr>>(k2eg->getGatewayCMDTopic(), start_snapshot_cmd));


    // wait for ack
    auto reply_msg_start_snapshot = k2eg->waitForReplyID(subscriber_reply, "rep-id", 60000);
    ASSERT_NE(reply_msg_start_snapshot, nullptr) << "Failed to get reply message";
    // get json object
    auto json_obj_start_snapshot = k2eg->getJsonObject(*reply_msg_start_snapshot);
    // check that the snapshot has been started
    ASSERT_EQ(json_obj_start_snapshot["error"].as_int64(), 0) << "JSON object 'error' is not 0";
    ASSERT_EQ(json_obj_start_snapshot["publishing_topic"].as_string(), SNAPSHOT_NAME) << "JSON object 'publishing_topic' is not 'snapshot_name'";

    // give a ticken to the maintanace task
    node_controller.performManagementTask();

    // list all snapshots from now up to two minutes ago using helper
    auto found_ids = k2eg->waitForSnapshotIdsInRange(storage_service);
    ASSERT_FALSE(found_ids.empty()) << "No snapshots found";

    // stop the snapshot
    auto stop_snapshot_cmd = MakeRepeatingSnapshotStopCommandShrdPtr(
        SerializationType::JSON,
        REPLY_TOPIC,
        "rep-id-1",
        SNAPSHOT_NAME);
    // send stop snapshot request
    k2eg->sendCommand(publisher, std::make_unique<CMDMessage<RepeatingSnapshotStopCommandShrdPtr>>(k2eg->getGatewayCMDTopic(), stop_snapshot_cmd));

    // wait for ack
    auto reply_msg_stop_snapshot = k2eg->waitForReplyID(subscriber_reply, "rep-id-1", 60000);
    ASSERT_NE(reply_msg_stop_snapshot, nullptr) << "Failed to get reply message";

    ASSERT_NO_THROW(k2eg.reset();) << "Failed to reset K2EG instance";
}
