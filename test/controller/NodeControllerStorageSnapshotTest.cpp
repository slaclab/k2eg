
#include "../NodeUtilities.h"
#include "k2eg/common/BaseSerialization.h"
#include "k2eg/controller/command/cmd/SnapshotCommand.h"
#include "k2eg/controller/node/NodeController.h"
#include "gtest/gtest.h"

int k2eg_controller_storage_snapshot_test_port = 20600;

using namespace k2eg::common;
using namespace k2eg::service::pubsub;
using namespace k2eg::controller::node;
using namespace k2eg::controller::command::cmd;

#define REPLY_TOPIC "app_reply_topic"
#define SNAPSHOT_NAME "snapshot_name"
TEST(NodeControllerStorageSnapshotTest, StartRecording)
{
    SubscriberInterfaceElementVector received_msg;
    auto k2eg = startK2EG(
        k2eg_controller_storage_snapshot_test_port,
        NodeType::FULL,
        true,
        true);
    ASSERT_NE(k2eg, nullptr) << "Failed to create K2EG instance";
    ASSERT_TRUE(k2eg->isRunning()) << "K2EG instance is not started";

    auto publisher = k2eg->getPublisherInstance();
    ASSERT_NE(publisher, nullptr) << "Failed to get publisher instance";

    auto subscriber = k2eg->getSubscriberInstance();
    subscriber->setQueue({REPLY_TOPIC, SNAPSHOT_NAME});
    ASSERT_NE(subscriber, nullptr) << "Failed to get subscriber instance";

    // start a snapshot
    auto snapshot_cmd = MakeRepeatingSnapshotCommandShrdPtr(
        SerializationType::JSON, 
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
    k2eg->sendCommand(publisher, std::make_unique<CMDMessage<RepeatingSnapshotCommandShrdPtr>>(k2eg->getGatewayCMDTopic(), snapshot_cmd));

    // wait for ack
    auto msg_vec = k2eg->getMessages(subscriber, 1);
    ASSERT_EQ(msg_vec.size(), 1);
    // get json object
    auto json_obj = k2eg->getJsonObject(*msg_vec[0]);

    ASSERT_NO_THROW(k2eg.reset();) << "Failed to reset K2EG instance";
}