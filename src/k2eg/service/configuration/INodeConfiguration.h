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
  // reset the configration on the start of the node
  const bool reset_on_start;
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


DEFINE_MMAP_FOR_TYPE(std::string, PVMonitorInfoShrdPtr, PVMonitorInfoMap)

/*
Is the cluster node configuration
*/
typedef struct {
  // the list of monitored PV names for the single node
  PVMonitorInfoMap pv_monitor_info_map;

  // check if the new PV name is already present for the specific configuration
  bool isPresent(const std::string& pv_nam, const PVMonitorInfo& info) const {
    auto range = pv_monitor_info_map.equal_range(pv_nam);
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second->event_serialization == info.event_serialization &&
            it->second->pv_destination_topic == info.pv_destination_topic) {
            return true;
        }
    }
    return false;
  }

  void removeFromKey(const std::string& key, const PVMonitorInfo& info) {
    auto range = pv_monitor_info_map.equal_range(key);
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second->event_serialization == info.event_serialization &&
            it->second->pv_destination_topic == info.pv_destination_topic) {
            pv_monitor_info_map.erase(it);
            break;
        }
    }
  }
}NodeConfiguration;
DEFINE_PTR_TYPES(NodeConfiguration)

// Function to convert a JSON object to a Config instance.
inline NodeConfigurationShrdPtr
config_from_json(const boost::json::object& obj) {
  auto cfg = std::make_shared<NodeConfiguration>();
  if (auto it = obj.if_contains("pv_monitor_map")) {
    if (it->is_array()) {
      const auto& arr = it->as_array();
      for (const auto& item : arr) {
        if (item.is_object()) {
          const auto& itemObj = item.as_object();
          std::string key = boost::json::value_to<std::string>(itemObj.at("key"));
          std::string destTopic = boost::json::value_to<std::string>(itemObj.at("dest"));
          std::uint8_t eventSerialization = 
              static_cast<std::uint8_t>(boost::json::value_to<int>(itemObj.at("ser")));
          auto pvMonitorInfo = std::make_shared<PVMonitorInfo>(PVMonitorInfo{destTopic, eventSerialization});
          cfg->pv_monitor_info_map.insert(PVMonitorInfoMapPair(key, pvMonitorInfo));
        }
      }
    }
  }
  return cfg;
}

// Function to convert a NodeConfiguration instance to a JSON object.
inline boost::json::object
config_to_json(const NodeConfiguration& cfg) {
  boost::json::object obj;
  boost::json::array arr;
  for (const auto& entry : cfg.pv_monitor_info_map) {
    boost::json::object itemObj;
    itemObj["key"] = entry.first;
    itemObj["dest"] = entry.second->pv_destination_topic;
    itemObj["ser"] = static_cast<int>(entry.second->event_serialization);
    arr.push_back(itemObj);
  }
  obj["pv_monitor_map"] = arr;
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

  virtual NodeConfigurationShrdPtr getNodeConfiguration() const                                        = 0;
  virtual bool                     setNodeConfiguration(NodeConfigurationShrdPtr node_configuration) = 0;
};
DEFINE_PTR_TYPES(INodeConfiguration)
}  // namespace k2eg::service::configuration

#endif  // K2EG_SERVICE_CONFIGURATION_INODECONFIGURATION_H_