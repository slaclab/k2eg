#include <gtest/gtest.h>
#include <k2eg/service/configuration/impl/consul/ConsulNodeConfiguration.h>
#include <k2eg/service/configuration/INodeConfiguration.h>
#include <memory>

using namespace k2eg::service::configuration;
using namespace k2eg::service::configuration::impl::consul;

TEST(ConsulConfiguration, Allocation) {
  // Check if the Consul client is created successfully
  ConstConfigurationServceiConfigUPtr config = std::make_unique<k2eg::service::configuration::ConfigurationServceiConfig>(
    ConfigurationServceiConfig{
        .config_server_host =  "consul",
        .config_server_port = 8500
    }
  );
  auto consul_config = std::make_shared<ConsuleNodeConfiguration>(std::move(config));
  ASSERT_NE(consul_config, nullptr);
  ASSERT_NO_THROW(consul_config.reset(););
}

TEST(ConsulConfiguration, SetAndGetNodeConfiguration) {
  // Check if the Consul client is created successfully
  ConstConfigurationServceiConfigUPtr config = std::make_unique<k2eg::service::configuration::ConfigurationServceiConfig>(
    ConfigurationServceiConfig{
        .config_server_host =  "consul",
        .config_server_port = 8500
    }
  );
  auto consul_config = std::make_shared<ConsuleNodeConfiguration>(std::move(config));
  ASSERT_NE(consul_config, nullptr);

  ASSERT_NO_THROW(consul_config->setNodeConfiguration(std::make_shared<NodeConfiguration>(
    NodeConfiguration{
      .pv_monitor_info_map = {
        {"pv-1", std::make_shared<PVMonitorInfo>(PVMonitorInfo{"topic-1", 1})},
        {"pv-2", std::make_shared<PVMonitorInfo>(PVMonitorInfo{"topic-2", 2})},
        {"pv-3", std::make_shared<PVMonitorInfo>(PVMonitorInfo{"topic-3", 3})}
    }
    }
  )));
  // retrive the configuration
  ConstNodeConfigurationShrdPtr node_config = nullptr;
  ASSERT_NO_THROW(node_config = consul_config->getNodeConfiguration(););
  ASSERT_NE(node_config, nullptr);
  ASSERT_EQ(node_config->pv_monitor_info_map.size(), 3);
  auto it = node_config->pv_monitor_info_map.begin();
  ASSERT_EQ(it++->first, "pv-1");
  ASSERT_EQ(it++->first, "pv-2");
  ASSERT_EQ(it++->first, "pv-3");

  // Update the configuration with an additional PV
  ASSERT_NO_THROW(consul_config->setNodeConfiguration(std::make_shared<NodeConfiguration>(
      NodeConfiguration{
          .pv_monitor_info_map = {
              {"pv-1", std::make_shared<PVMonitorInfo>(PVMonitorInfo{"topic-1", 1})},
              {"pv-2", std::make_shared<PVMonitorInfo>(PVMonitorInfo{"topic-2", 2})},
              {"pv-3", std::make_shared<PVMonitorInfo>(PVMonitorInfo{"topic-3", 3})},
              {"pv-4", std::make_shared<PVMonitorInfo>(PVMonitorInfo{"topic-4", 4})}
          }
      }
  )));
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