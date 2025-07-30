#include <gtest/gtest.h>
#include <k2eg/service/configuration/INodeConfiguration.h>
#include <k2eg/service/configuration/impl/consul/ConsulNodeConfiguration.h>
#include <memory>

using namespace k2eg::service::configuration;
using namespace k2eg::service::configuration::impl::consul;

TEST(ConsulConfigurationSnapshotGatewayTest, SnapshotConfiguration)
{
    // Setup configuration (adjust as needed for your test environment)
    auto config = std::make_unique<ConfigurationServiceConfig>(
        ConfigurationServiceConfig{
            "consul", // config_server_host
            8500,     // config_server_port
            true      // reset_on_start
        });

    ConsulNodeConfiguration nodeConfig(std::move(config));

    std::string snapshot_id = "test-snapshot";

    // Clean up before test
    SnapshotConfigurationShrdPtr snapshot_config = MakeSnapshotConfigurationShrdPtr(
        SnapshotConfiguration{
            .weight = 100,
            .weight_unit = "eps",
            .running_status = false,
            .archiving_status = false,
            .archiver_id = "",
            .update_timestamp = "2023-10-01T00:00:00Z"});
    ASSERT_TRUE(nodeConfig.setSnapshotConfiguration(snapshot_id, snapshot_config));
    ASSERT_FALSE(nodeConfig.isSnapshotRunning(snapshot_id));

    // retrieve the snapshot configuration
    ConstSnapshotConfigurationShrdPtr retrieved_config = nodeConfig.getSnapshotConfiguration(snapshot_id);
    ASSERT_NE(retrieved_config, nullptr);
    ASSERT_EQ(retrieved_config->weight, 100);
    ASSERT_EQ(retrieved_config->weight_unit, "eps");
    ASSERT_EQ(retrieved_config->running_status, false);
    ASSERT_EQ(retrieved_config->archiving_status, false);
    ASSERT_EQ(retrieved_config->archiver_id, "");
    ASSERT_STREQ(retrieved_config->update_timestamp.c_str(), "2023-10-01T00:00:00Z");
}

TEST(ConsulConfigurationSnapshotGatewayTest, CheckSnapshotAcquireRelease)
{
    // Setup configuration (adjust as needed for your test environment)
    auto config = std::make_unique<ConfigurationServiceConfig>(
        ConfigurationServiceConfig{
            "consul", // config_server_host
            8500,     // config_server_port
            true      // reset_on_start
        });

    ConsulNodeConfiguration nodeConfig(std::move(config));

    std::string snapshot_id = "test-snapshot";

    // Clean up before test
    nodeConfig.deleteSnapshotConfiguration(snapshot_id);

    // Try to acquire snapshot lock
    ASSERT_TRUE(nodeConfig.tryAcquireSnapshot(snapshot_id, true));

    // Gateway should match
    ASSERT_STREQ(nodeConfig.getSnapshotGateway(snapshot_id).c_str(), nodeConfig.getNodeName().c_str());

    // Should appear in snapshots by gateway
    auto by_gateway = nodeConfig.getSnapshots();
    ASSERT_NE(std::find(by_gateway.begin(), by_gateway.end(), snapshot_id), by_gateway.end());

    // Release the snapshot
    ASSERT_TRUE(nodeConfig.releaseSnapshot(snapshot_id, true));

    // Clean up after test
    nodeConfig.deleteSnapshotConfiguration(snapshot_id);
}

TEST(ConsulConfigurationSnapshotGatewayTest, CheckSnapshotAcquireReleaseConcurrency)
{
    // Setup configuration (adjust as needed for your test environment)
    auto configOne = std::make_unique<ConfigurationServiceConfig>(
        ConfigurationServiceConfig{
            "consul", // config_server_host
            8500,     // config_server_port
            true      // reset_on_start
        });
    auto configTwo = std::make_unique<ConfigurationServiceConfig>(
        ConfigurationServiceConfig{
            "consul", // config_server_host
            8500,     // config_server_port
            true      // reset_on_start
        });

    ConsulNodeConfiguration nodeConfigOne(std::move(configOne));
    ConsulNodeConfiguration nodeConfigTwo(std::move(configTwo));
    std::string             snapshot_id = "test-snapshot";

    // Clean up before test

    // Try to acquire snapshot lock
    ASSERT_TRUE(nodeConfigOne.tryAcquireSnapshot(snapshot_id, true));
    ASSERT_FALSE(nodeConfigTwo.tryAcquireSnapshot(snapshot_id, true)); // Should fail since already acquired

    // Release the snapshot
    ASSERT_TRUE(nodeConfigOne.releaseSnapshot(snapshot_id, true));
    ASSERT_TRUE(nodeConfigTwo.tryAcquireSnapshot(snapshot_id, true)); // Now should succeed
    // Clean up after test
    ASSERT_TRUE(nodeConfigOne.releaseSnapshot(snapshot_id, true));
}

TEST(ConsulConfigurationSnapshotGatewayTest, CheckSnapshotAcquireReleaseConcurrencyOnExpiration)
{
    // Setup configuration (adjust as needed for your test environment)
    auto configOne = std::make_unique<ConfigurationServiceConfig>(
        ConfigurationServiceConfig{
            "consul", // config_server_host
            8500,     // config_server_port
            true      // reset_on_start
        });
    auto configTwo = std::make_unique<ConfigurationServiceConfig>(
        ConfigurationServiceConfig{
            "consul", // config_server_host
            8500,     // config_server_port
            true      // reset_on_start
        });
    ConsulNodeConfigurationUPtr nodeConfigOne = MakeConsulNodeConfigurationUPtr(std::move(configOne));
    ConsulNodeConfigurationUPtr nodeConfigTwo = MakeConsulNodeConfigurationUPtr(std::move(configTwo));
    std::string                 snapshot_id = "test-snapshot";

    // Clean up before test

    // Try to acquire snapshot lock
    ASSERT_TRUE(nodeConfigOne->tryAcquireSnapshot(snapshot_id, true));
    ASSERT_FALSE(nodeConfigTwo->tryAcquireSnapshot(snapshot_id, true)); // Should fail since already acquired

    // Now delete the config one to simulate the session expiration
    nodeConfigOne.reset();
    // wait 30 seconds to ensure the session is expired
    std::this_thread::sleep_for(std::chrono::seconds(30));
    // Now the second node should be able to acquire the snapshot
    ASSERT_TRUE(nodeConfigTwo->tryAcquireSnapshot(snapshot_id, true));
    // Clean up after test
    ASSERT_TRUE(nodeConfigTwo->releaseSnapshot(snapshot_id, true));
}