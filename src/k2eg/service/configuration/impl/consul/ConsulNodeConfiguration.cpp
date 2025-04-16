#include <k2eg/common/utility.h>
#include <k2eg/service/configuration/INodeConfiguration.h>
#include <k2eg/service/configuration/impl/consul/ConsulNodeConfiguration.h>

#include <memory>
#include <oatpp/core/base/Environment.hpp>
#include <oatpp/network/tcp/client/ConnectionProvider.hpp>
#include <oatpp/web/client/HttpRequestExecutor.hpp>

using namespace k2eg::service::configuration;
using namespace k2eg::service::configuration::impl::consul;

using namespace oatpp::base;
using namespace oatpp::data::mapping;
using namespace oatpp::network;
using namespace oatpp::network::tcp::client;

using namespace oatpp::consul;

ConsuleNodeConfiguration::ConsuleNodeConfiguration(ConstConfigurationServceiConfigUPtr _config) : INodeConfiguration(std::move(_config)) {
  // Initialize Oat++ environment.
  oatpp::base::Environment::init();

  oatpp::String hostname = oatpp::String(config->config_server_host);
  v_uint16 port = config->config_server_port;
  // Create a TCP connection provider and an HTTP request executor for Consul.
  auto connectionProvider = oatpp::network::tcp::client::ConnectionProvider::createShared({hostname, port,oatpp::network::Address::Family::IP_4});
  auto requestExecutor    = oatpp::web::client::HttpRequestExecutor::createShared(connectionProvider);

  // Create the Consul client instance.
  client = Client::createShared(requestExecutor);
  if (!client) { throw std::runtime_error("Failed to create Consul client"); }

  // compos the configuration key
  node_configuration_key = getNodeKey();

  if(config->reset_on_start) {
    // remove the old configuration
    client->kvDelete(node_configuration_key);
  }
  // in case this is the first time we are running check if we have a configuration
  // and if not create a new one
  try {
    auto json_value = client->kvGet(node_configuration_key);
  } catch (Client::Error& ex) {
    // create a new configuration
    setNodeConfiguration(std::make_shared<NodeConfiguration>());
  }
};

ConsuleNodeConfiguration::~ConsuleNodeConfiguration() {
  // Shutdown Oat++ environment.
  oatpp::base::Environment::destroy();
}

std::string
ConsuleNodeConfiguration::getNodeKey() {
  char hostname[HOST_NAME_MAX];
  if (gethostname(hostname, sizeof(hostname)) == 0) { return STRING_FORMAT("k2eg-node/%1%/configuration", std::string(hostname)); }
  const char* envHostname = std::getenv("HOSTNAME");
  if (envHostname == nullptr) { throw std::runtime_error("Failed to get hostname"); }
  return STRING_FORMAT("k2eg-node/%1%/configuration", std::string(envHostname));
}

NodeConfigurationShrdPtr
ConsuleNodeConfiguration::getNodeConfiguration() const {
  oatpp::String json_value = client->kvGet(node_configuration_key);
  if (json_value == nullptr) { return nullptr; }
  auto json_str = json_value.getValue("");
  try {
    // Parse the JSON string using Boost.JSON.
    boost::json::value  parsed = boost::json::parse(json_str);
    boost::json::object obj    = parsed.as_object();
    return config_from_json(obj);
  } catch (const std::exception& ex) { throw std::runtime_error(STRING_FORMAT("Failed to parse JSON: %1%", ex.what())); }
}

bool
ConsuleNodeConfiguration::setNodeConfiguration(NodeConfigurationShrdPtr node_configuration) {
  // store configuration in Consul KV store
  auto json_obj = config_to_json(*node_configuration);
  auto json_str = boost::json::serialize(json_obj);
  auto res = client->kvPut(node_configuration_key, json_str);
  return res;
}