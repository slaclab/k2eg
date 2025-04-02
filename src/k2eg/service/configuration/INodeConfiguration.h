#ifndef K2EG_SERVICE_CONFIGURATION_INODECONFIGURATION_H_
#define K2EG_SERVICE_CONFIGURATION_INODECONFIGURATION_H_

#include <k2eg/common/types.h>

#include <boost/json.hpp>
#include <cstdint>
#include <stdint.h>

namespace k2eg::service::configuration {

/*
is the configuration service config
*/
struct ConfigurationServceiConfig {
  std::string config_server_host;
  std::int16_t config_server_port;
};
DEFINE_PTR_TYPES(ConfigurationServceiConfig)

/*
Is the node configuration
*/
struct NodeConfiguration {
  std::vector<std::string> pv_name_list;
};
DEFINE_PTR_TYPES(NodeConfiguration)

// Function to convert a JSON object to a Config instance.
inline ConstNodeConfigurationUPtr
config_from_json(const boost::json::object& obj) {
  auto cfg = std::make_unique<NodeConfiguration>();
  // Use value_to to extract values from the JSON object.
  // cfg.host   = json::value_to<std::string>(obj.at("host"));
  // cfg.port   = json::value_to<int>(obj.at("port"));
  // cfg.use_ssl = json::value_to<bool>(obj.at("use_ssl"));
  return cfg;
}

// Function to convert a Config instance to a JSON object.
inline boost::json::object
config_to_json(const NodeConfiguration& cfg) {
  boost::json::object obj;
  // obj["host"]    = cfg.host;
  // obj["port"]    = cfg.port;
  // obj["use_ssl"] = cfg.use_ssl;
  return obj;
}

/*
The INodeConfiguration interface defines the base logic for node configuration services.
*/
class INodeConfiguration {
 protected:
  ConstConfigurationServceiConfigUPtr config;

 public:
  INodeConfiguration(ConstConfigurationServceiConfigUPtr config) : config(std::move(config)) {};
  virtual ~INodeConfiguration() = default;

  virtual ConstNodeConfigurationUPtr getNodeConfiguration() const                                        = 0;
  virtual void                       setNodeConfiguration(ConstNodeConfigurationUPtr node_configuration) = 0;
};

}  // namespace k2eg::service::configuration

#endif  // K2EG_SERVICE_CONFIGURATION_INODECONFIGURATION_H_