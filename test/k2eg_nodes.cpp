#include <gtest/gtest.h>
#include "NodeUtilities.h"

int k2eg_node_tests_tcp_port = 10000;

TEST(K2EGNode, StartStopGateway)
{
    // start the gateway
    auto k2eg = startK2EG(
        k2eg_node_tests_tcp_port,
        true,
        true,
        true);
    ASSERT_NE(k2eg, nullptr) << "Failed to create K2EG instance";
    ASSERT_TRUE(k2eg->isRunning()) << "K2EG instance is not started";
    ASSERT_NO_THROW(k2eg.reset();) << "Failed to reset K2EG instance";
}

TEST(K2EGNode, StartStopStorage)
{
    // start the storage
    auto k2eg = startK2EG(
        k2eg_node_tests_tcp_port,
        false,
        true,
        true);
    ASSERT_NE(k2eg, nullptr) << "Failed to create K2EG instance";
    ASSERT_TRUE(k2eg->isRunning()) << "K2EG instance is not started";
    ASSERT_NO_THROW(k2eg.reset();) << "Failed to reset K2EG instance";
}