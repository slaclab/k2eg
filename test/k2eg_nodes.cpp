#include <gtest/gtest.h>
#include "NodeUtilities.h"
#include "k2eg/controller/node/NodeController.h"

int k2eg_node_tests_tcp_port = 10000;
using namespace k2eg::controller::node;

TEST(K2EGNode, StartStopGateway)
{
    // start the node as gateway
    auto k2eg = startK2EG(
        k2eg_node_tests_tcp_port,
        NodeType::GATEWAY,
        true,
        true);
    ASSERT_NE(k2eg, nullptr) << "Failed to create K2EG instance";
    ASSERT_TRUE(k2eg->isRunning()) << "K2EG instance is not started";
    ASSERT_NO_THROW(k2eg.reset();) << "Failed to reset K2EG instance";
}

TEST(K2EGNode, StartStopStorage)
{
    // start the node as storage
    auto k2eg = startK2EG(
        k2eg_node_tests_tcp_port,
        NodeType::STORAGE,
        true,
        true);
    ASSERT_NE(k2eg, nullptr) << "Failed to create K2EG instance";
    ASSERT_TRUE(k2eg->isRunning()) << "K2EG instance is not started";
    ASSERT_NO_THROW(k2eg.reset();) << "Failed to reset K2EG instance";
}

TEST(K2EGNode, StartStopFull)
{
    // start the node as full
    auto k2eg = startK2EG(
        k2eg_node_tests_tcp_port,
        NodeType::FULL,
        true,
        true);
    ASSERT_NE(k2eg, nullptr) << "Failed to create K2EG instance";
    ASSERT_TRUE(k2eg->isRunning()) << "K2EG instance is not started";
    ASSERT_NO_THROW(k2eg.reset();) << "Failed to reset K2EG instance";
}