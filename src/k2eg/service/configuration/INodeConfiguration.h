#ifndef K2EG_SERVICE_CONFIGURATION_INODECONFIGURATION_H_
#define K2EG_SERVICE_CONFIGURATION_INODECONFIGURATION_H_

#include <k2eg/common/types.h>

#include <boost/json.hpp>
#include <cstdint>
#include <memory>
#include <stdint.h>
#include <map>

namespace k2eg::service::configuration {

/*
is the configuration service config
*/
struct ConfigurationServceiConfig {
  const std::string config_server_host;
  const std::int16_t config_server_port;
};
DEFINE_PTR_TYPES(ConfigurationServceiConfig)

/// The informazion about serialziation and destination topic for a single PV
typedef struct {
  // serialization type
  const std::string pv_destination_topic;
  // destination topic
  const std::uint8_t event_serialization;
}PVMonitorInfo;
DEFINE_PTR_TYPES(PVMonitorInfo)


DEFINE_MAP_FOR_TYPE(std::string, PVMonitorInfoShrdPtr, PVMonitorInfoMap)

/*
Is the cluster node configuration
*/
typedef struct {
  // the list of monitored PV names for the single node
  PVMonitorInfoMap pv_monitor_info_map;
}NodeConfiguration;
DEFINE_PTR_TYPES(NodeConfiguration)

// Function to convert a JSON object to a Config instance.
inline ConstNodeConfigurationShrdPtr
config_from_json(const boost::json::object& obj) {
  auto cfg = std::make_shared<NodeConfiguration>();
  if(auto it = obj.if_contains("pv_monitor_map")) {
    if(it->is_object()) {
      const auto& mapObj = it->as_object();
      for(const auto& kv : mapObj) {
        // Parse PVMonitorInfo from the JSON object value.
        const auto& pvObj = kv.value().as_object();
        std::string destTopic = boost::json::value_to<std::string>(pvObj.at("dest"));
        std::uint8_t eventSerialization = static_cast<std::uint8_t>(boost::json::value_to<int>(pvObj.at("ser")));
        auto pvMonitorInfo = std::make_shared<PVMonitorInfo>(PVMonitorInfo{destTopic, eventSerialization});
        cfg->pv_monitor_info_map.insert(PVMonitorInfoMapPair(kv.key(), pvMonitorInfo));
      }
    }
  }
  return cfg;
}

// Function to convert a Config instance to a JSON object.
inline boost::json::object
config_to_json(const NodeConfiguration& cfg) {
  boost::json::object obj;
  boost::json::object mapObj;
  for (const auto& kv : cfg.pv_monitor_info_map) {
    boost::json::object pvObj;
    // Convert the PVMonitorInfo pointed by kv.second.
    pvObj["dest"] = kv.second->pv_destination_topic;
    pvObj["ser"] = kv.second->event_serialization;
    mapObj[kv.first] = std::move(pvObj);
  }
  obj["pv_monitor_map"] = std::move(mapObj);
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
DEFINE_PTR_TYPES(INodeConfiguration)
}  // namespace k2eg::service::configuration

#endif  // K2EG_SERVICE_CONFIGURATION_INODECONFIGURATION_H_