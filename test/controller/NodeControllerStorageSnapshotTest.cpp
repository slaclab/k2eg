
#include "../NodeUtilities.h"
#include "k2eg/common/BaseSerialization.h"
#include "k2eg/controller/command/cmd/SnapshotCommand.h"
#include "k2eg/controller/node/NodeController.h"

int k2eg_controller_storage_snapshot_test_port = 20600;

using namespace k2eg::controller::node;
using namespace k2eg::controller::command::cmd;

TEST(NodeControllerStorageSnapshotTest, StartRecording)
{
    auto k2eg = startK2EG(
        k2eg_controller_storage_snapshot_test_port,
        NodeType::FULL,
        true,
        true);
    ASSERT_NE(k2eg, nullptr) << "Failed to create K2EG instance";
    ASSERT_TRUE(k2eg->isRunning()) << "K2EG instance is not started";

    auto publisher = k2eg->getPublisherInstance();
    ASSERT_NE(publisher, nullptr) << "Failed to get publisher instance";

    // start a snapshot
    auto snapshot_cmd = MakeRepeatingSnapshotCommandShrdPtr();
    snapshot_cmd->snapshot_name = "test_snapshot";
    snapshot_cmd->pv_name_list = {"pva://variable:a", "pva://variable:b"};
    snapshot_cmd->reply_topic = "app_reply_topic";
    snapshot_cmd->reply_id = "rep-id";
    snapshot_cmd->repeat_delay_msec = 1000;
    snapshot_cmd->sub_push_delay_msec = 500;
    snapshot_cmd->serialization = k2eg::common::SerializationType::JSON;

    k2eg->sendCommand(publisher, std::make_unique<CMDMessage<RepeatingSnapshotCommandShrdPtr>>(k2eg->getGatewayCMDTopic(), snapshot_cmd));

    auto subscriber = k2eg->getSubscriberInstance();
    ASSERT_NE(subscriber, nullptr) << "Failed to get subscriber instance";

    ASSERT_NO_THROW(k2eg.reset();) << "Failed to reset K2EG instance";
}