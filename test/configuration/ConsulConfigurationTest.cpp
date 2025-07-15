#include <gtest/gtest.h>
#include <k2eg/service/configuration/INodeConfiguration.h>
#include <k2eg/service/configuration/impl/consul/ConsulNodeConfiguration.h>
#include <memory>

using namespace k2eg::service::configuration;
using namespace k2eg::service::configuration::impl::consul;

TEST(ConsulConfiguration, Allocation)
{
    // Check if the Consul client is created successfully
    ConstConfigurationServiceConfigUPtr config = std::make_unique<k2eg::service::configuration::ConfigurationServiceConfig>(
        ConfigurationServiceConfig{
            .config_server_host = "consul",
            .config_server_port = 8500});
    auto consul_config = std::make_shared<ConsulNodeConfiguration>(std::move(config));
    ASSERT_NE(consul_config, nullptr);
    ASSERT_NO_THROW(consul_config.reset(););
}

TEST(ConsulConfiguration, SetAndGetNodeConfiguration)
{
    // Check if the Consul client is created successfully
    ConstConfigurationServiceConfigUPtr config = std::make_unique<k2eg::service::configuration::ConfigurationServiceConfig>(
        ConfigurationServiceConfig{
            .config_server_host = "consul",
            .config_server_port = 8500});
    auto consul_config = std::make_shared<ConsulNodeConfiguration>(std::move(config));
    ASSERT_NE(consul_config, nullptr);

    ASSERT_NO_THROW(consul_config->setNodeConfiguration(std::make_shared<NodeConfiguration>(
        NodeConfiguration{
            .pv_monitor_info_map = {
                {"pv-1", std::make_shared<PVMonitorInfo>(PVMonitorInfo{"topic-1", 1})},
                {"pv-2", std::make_shared<PVMonitorInfo>(PVMonitorInfo{"topic-1", 1})},
                {"pv-2", std::make_shared<PVMonitorInfo>(PVMonitorInfo{"topic-2", 2})},
                {"pv-3", std::make_shared<PVMonitorInfo>(PVMonitorInfo{"topic-3", 3})}}})));
    // retrive the configuration
    ConstNodeConfigurationShrdPtr node_config = nullptr;
    ASSERT_NO_THROW(node_config = consul_config->getNodeConfiguration(););
    ASSERT_NE(node_config, nullptr);
    ASSERT_EQ(node_config->pv_monitor_info_map.size(), 4);
    auto it = node_config->pv_monitor_info_map.begin();
    ASSERT_EQ(it++->first, "pv-1");
    ASSERT_EQ(it++->first, "pv-2");
    ASSERT_EQ(it++->first, "pv-2");
    ASSERT_EQ(it++->first, "pv-3");

    // Update the configuration with an additional PV
    ASSERT_NO_THROW(consul_config->setNodeConfiguration(std::make_shared<NodeConfiguration>(
        NodeConfiguration{
            .pv_monitor_info_map = {
                {"pv-1", std::make_shared<PVMonitorInfo>(PVMonitorInfo{"topic-1", 1})},
                {"pv-2", std::make_shared<PVMonitorInfo>(PVMonitorInfo{"topic-2", 2})},
                {"pv-3", std::make_shared<PVMonitorInfo>(PVMonitorInfo{"topic-3", 3})},
                {"pv-4", std::make_shared<PVMonitorInfo>(PVMonitorInfo{"topic-4", 4})}}})));
    ASSERT_NO_THROW(node_config = consul_config->getNodeConfiguration());
    ASSERT_NE(node_config, nullptr);
    ASSERT_EQ(node_config->pv_monitor_info_map.size(), 4);

    it = node_config->pv_monitor_info_map.begin();
    ASSERT_EQ(it++->first, "pv-1");
    ASSERT_EQ(it++->first, "pv-2");
    ASSERT_EQ(it++->first, "pv-3");
    ASSERT_EQ(it++->first, "pv-4");

    ASSERT_NO_THROW(consul_config.reset());
}

TEST(ConsulConfiguration, SnapshotConfiguration)
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
            .timestamp = "2023-10-01T00:00:00Z"});
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
    ASSERT_EQ(retrieved_config->timestamp, "2023-10-01T00:00:00Z");
}

TEST(ConsulConfiguration, CheckSnapshotAcquireRelease)
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
    ASSERT_TRUE(nodeConfig.tryAcquireSnapshot(snapshot_id));

    // Gateway should match
    ASSERT_STREQ(nodeConfig.getSnapshotGateway(snapshot_id).c_str(), nodeConfig.getNodeName().c_str());

    // Should appear in snapshots by gateway
    auto by_gateway = nodeConfig.getSnapshots();
    ASSERT_NE(std::find(by_gateway.begin(), by_gateway.end(), snapshot_id), by_gateway.end());

    // Release the snapshot
    ASSERT_TRUE(nodeConfig.releaseSnapshot(snapshot_id));

    // Clean up after test
    nodeConfig.deleteSnapshotConfiguration(snapshot_id);
}

TEST(ConsulConfiguration, CheckSnapshotAcquireReleaseConcurrency)
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
    ASSERT_TRUE(nodeConfigOne.tryAcquireSnapshot(snapshot_id));
    ASSERT_FALSE(nodeConfigTwo.tryAcquireSnapshot(snapshot_id)); // Should fail since already acquired

    // Release the snapshot
    ASSERT_TRUE(nodeConfigOne.releaseSnapshot(snapshot_id));
    ASSERT_TRUE(nodeConfigTwo.tryAcquireSnapshot(snapshot_id)); // Now should succeed
    // Clean up after test
    ASSERT_TRUE(nodeConfigOne.releaseSnapshot(snapshot_id));
}

TEST(ConsulConfiguration, CheckSnapshotAcquireReleaseConcurrencyOnExpiration)
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
    ASSERT_TRUE(nodeConfigOne->tryAcquireSnapshot(snapshot_id));
    ASSERT_FALSE(nodeConfigTwo->tryAcquireSnapshot(snapshot_id)); // Should fail since already acquired

    // Now delete the con fig one to simualte the session expiration
    nodeConfigOne.reset();
    // wait 30 seconds to ensure the session is expired
    std::this_thread::sleep_for(std::chrono::seconds(30));
    // Now the second node should be able to acquire the snapshot
    ASSERT_TRUE(nodeConfigTwo->tryAcquireSnapshot(snapshot_id));
    // Clean up after test
    ASSERT_TRUE(nodeConfigTwo->releaseSnapshot(snapshot_id));
}