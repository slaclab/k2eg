
#include <gtest/gtest.h>
#include "../NodeUtilities.h"
#include "k2eg/controller/node/NodeController.h"

int k2eg_controller_storage_snapshot_test_port = 20600;

using namespace k2eg::controller::node;

TEST(NodeControllerStorageSnapshotTest, SnapshotCommandMsgPackSer)
{
     auto k2eg = startK2EG(
        k2eg_controller_storage_snapshot_test_port,
        NodeType::FULL,
        true,
        true);
    ASSERT_NE(k2eg, nullptr) << "Failed to create K2EG instance";
    ASSERT_TRUE(k2eg->isRunning()) << "K2EG instance is not started";
    ASSERT_NO_THROW(k2eg.reset();) << "Failed to reset K2EG instance";
}