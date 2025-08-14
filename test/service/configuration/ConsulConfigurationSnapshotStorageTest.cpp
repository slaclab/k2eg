#include <gtest/gtest.h>
#include <k2eg/service/configuration/INodeConfiguration.h>
#include <k2eg/service/configuration/impl/consul/ConsulNodeConfiguration.h>
#include <memory>

using namespace k2eg::service::configuration;
using namespace k2eg::service::configuration::impl::consul;

TEST(ConsulConfigurationSnapshotStorageTest, SnapshotConfiguration)
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

    // now simulate the storage node that take ownership of the snapshot
    ASSERT_TRUE(nodeConfig.tryAcquireSnapshot(snapshot_id, false)); 

    // get the status
    auto snapshot_config = nodeConfig.getSnapshotArchiveStatus(snapshot_id);
}
