#ifndef K2EG_SERVICE_CONFIGURATION_INODECONFIGURATION_H_
#define K2EG_SERVICE_CONFIGURATION_INODECONFIGURATION_H_

#include <k2eg/common/types.h>

#include <boost/json.hpp>
#include <cstdint>
#include <memory>
#include <stdint.h>

namespace k2eg::service::configuration {

/*
is the configuration service config
*/
struct ConfigurationServceiConfig {
  const std::string config_server_host;
  const std::int16_t config_server_port;
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
inline ConstNodeConfigurationShrdPtr
config_from_json(const boost::json::object& obj) {
  auto cfg = std::make_shared<NodeConfiguration>();
  // read the pv_name_list vector from json 
  if (auto v = obj.if_contains("pv_name_list")) {
    auto json_array = v->as_array();
    // find all stirng in the vector
    for (auto& element : json_array) {
      if (element.kind() != boost::json::kind::string) continue;
      cfg->pv_name_list.push_back(boost::json::value_to<std::string>(element));
    }
  }
  return cfg;
}

// Function to convert a Config instance to a JSON object.
inline boost::json::object
config_to_json(const NodeConfiguration& cfg) {
  boost::json::object obj;
  // write the pv_name_list vector to json
  boost::json::array json_array;
  for (const auto& name : cfg.pv_name_list) {
    json_array.emplace_back(name);
  }
  obj["pv_name_list"] = std::move(json_array);
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

  virtual ConstNodeConfigurationShrdPtr getNodeConfiguration() const                                        = 0;
  virtual bool                       setNodeConfiguration(ConstNodeConfigurationShrdPtr node_configuration) = 0;
};

}  // namespace k2eg::service::configuration

#endif  // K2EG_SERVICE_CONFIGURATION_INODECONFIGURATION_H_