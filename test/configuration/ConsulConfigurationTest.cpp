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
        .pv_name_list = {"pv-1", "pv-2", "pv-3"}
    }
  )));
  // retrive the configuration
  ConstNodeConfigurationShrdPtr node_config = nullptr;
  ASSERT_NO_THROW(node_config = consul_config->getNodeConfiguration(););
  ASSERT_NE(node_config, nullptr);
  ASSERT_EQ(node_config->pv_name_list.size(), 3);
  ASSERT_EQ(node_config->pv_name_list[0], "pv-1");
  ASSERT_EQ(node_config->pv_name_list[1], "pv-2");
  ASSERT_EQ(node_config->pv_name_list[2], "pv-3");

  // update values
  ASSERT_NO_THROW(consul_config->setNodeConfiguration(std::make_shared<NodeConfiguration>(
    NodeConfiguration{
        .pv_name_list = {"pv-1", "pv-2", "pv-3", "pv-4"}
    }
  )));
  ASSERT_NO_THROW(node_config = consul_config->getNodeConfiguration(););
  ASSERT_NE(node_config, nullptr);
  ASSERT_EQ(node_config->pv_name_list.size(), 3);
  ASSERT_EQ(node_config->pv_name_list[0], "pv-1");
  ASSERT_EQ(node_config->pv_name_list[1], "pv-2");
  ASSERT_EQ(node_config->pv_name_list[2], "pv-3");
  ASSERT_EQ(node_config->pv_name_list[3], "pv-4");

  ASSERT_NO_THROW(consul_config.reset(););
}