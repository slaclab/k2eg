#include <k2eg/service/configuration/impl/consul/ConsulNodeConfiguration.h>

#include <oatpp/core/base/Environment.hpp>
#include <oatpp/network/tcp/client/ConnectionProvider.hpp>
#include <oatpp/web/client/HttpRequestExecutor.hpp>
#include <oatpp-consul/Client.hpp>

using namespace k2eg::service::configuration;
using namespace k2eg::service::configuration::impl::consul;

using namespace oatpp::base;
using namespace oatpp::data::mapping;
using namespace oatpp::network;
using namespace oatpp::network::tcp::client;

using namespace oatpp::consul;

ConsuleNodeConfiguration::ConsuleNodeConfiguration(ConstConfigurationServceiConfigUPtr config) : INodeConfiguration(std::move(config)) {
    // Initialize Oat++ environment.
    oatpp::base::Environment::init();

      // Create a TCP connection provider and an HTTP request executor for Consul.
  auto connectionProvider = oatpp::network::tcp::client::ConnectionProvider::createShared(Address(config->config_server_host, config->config_server_port));
  auto requestExecutor = oatpp::web::client::HttpRequestExecutor::createShared(connectionProvider);
  
  // Create the Consul client instance.
  auto consulClient = Client::createShared(requestExecutor);
};

ConsuleNodeConfiguration::~ConsuleNodeConfiguration() {
  // Shutdown Oat++ environment.
  oatpp::base::Environment::destroy();
}

ConstNodeConfigurationUPtr
ConsuleNodeConfiguration::getNodeConfiguration() const {
  return std::make_unique<NodeConfiguration>();
}

void
ConsuleNodeConfiguration::setNodeConfiguration(ConstNodeConfigurationUPtr node_configuration){
    
}