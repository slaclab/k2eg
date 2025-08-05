#include <boost/json.hpp>
#include <k2eg/service/configuration/INodeConfiguration.h>

using namespace k2eg::service::configuration;

bool NodeConfiguration::isPresent(const std::string& pv_nam, const PVMonitorInfo& info) const
{
    auto range = pv_monitor_info_map.equal_range(pv_nam);
    for (auto it = range.first; it != range.second; ++it)
    {
        if (it->second->event_serialization == info.event_serialization && it->second->pv_destination_topic == info.pv_destination_topic)
        {
            return true;
        }
    }
    return false;
}

void NodeConfiguration::removeFromKey(const std::string& key, const PVMonitorInfo& info)
{
    auto range = pv_monitor_info_map.equal_range(key);
    for (auto it = range.first; it != range.second; ++it)
    {
        if (it->second->event_serialization == info.event_serialization && it->second->pv_destination_topic == info.pv_destination_topic)
        {
            pv_monitor_info_map.erase(it);
            break;
        }
    }
}

std::string NodeConfiguration::toJson(const NodeConfiguration& config)
{
    boost::json::object obj;
    boost::json::array  arr;
    for (const auto& entry : config.pv_monitor_info_map)
    {
        boost::json::object itemObj;
        itemObj["key"] = entry.first;
        itemObj["dest"] = entry.second->pv_destination_topic;
        itemObj["ser"] = static_cast<int>(entry.second->event_serialization);
        arr.push_back(itemObj);
    }
    obj["pv_monitor_map"] = arr;
    return boost::json::serialize(obj);
}

NodeConfiguration NodeConfiguration::fromJson(const std::string& json_str)
{
    NodeConfiguration config;
    if (json_str.empty())
    {
        return config;
    }
    boost::json::value  jv = boost::json::parse(json_str);
    boost::json::object obj = jv.as_object();
    if (auto it = obj.if_contains("pv_monitor_map"))
    {
        if (it->is_array())
        {
            const auto& arr = it->as_array();
            for (const auto& item : arr)
            {
                if (item.is_object())
                {
                    const auto&  itemObj = item.as_object();
                    std::string  key = boost::json::value_to<std::string>(itemObj.at("key"));
                    std::string  destTopic = boost::json::value_to<std::string>(itemObj.at("dest"));
                    std::uint8_t eventSerialization = static_cast<std::uint8_t>(boost::json::value_to<int>(itemObj.at("ser")));
                    auto         pvMonitorInfo = std::make_shared<PVMonitorInfo>(PVMonitorInfo{destTopic, eventSerialization});
                    config.pv_monitor_info_map.insert(PVMonitorInfoMapPair(key, pvMonitorInfo));
                }
            }
        }
    }
    return config;
}

// Provide out-of-line definition for INodeConfiguration destructor to emit vtable

INodeConfiguration::INodeConfiguration(ConstConfigurationServiceConfigUPtr config)
    : config(std::move(config)){};

INodeConfiguration::~INodeConfiguration() {}

// SnapshotConfiguration implementation
std::string SnapshotConfiguration::toJson(const SnapshotConfiguration& config)
{
    boost::json::object obj;
    obj["weight"] = config.weight;
    obj["weight_unit"] = config.weight_unit;
    obj["update_timestamp"] = config.update_timestamp;
    obj["config_json"] = boost::json::value(config.config_json.empty() ? boost::json::array() : boost::json::parse(config.config_json));
    return boost::json::serialize(obj);
}

SnapshotConfiguration SnapshotConfiguration::fromJson(const std::string& json_str)
{
    SnapshotConfiguration config;
    if (json_str.empty())
    {
        return config;
    }
    boost::json::value  jv = boost::json::parse(json_str);
    boost::json::object obj = jv.as_object();
    config.weight = boost::json::value_to<int>(obj.at("weight"));
    config.weight_unit = boost::json::value_to<std::string>(obj.at("weight_unit"));
    config.update_timestamp = boost::json::value_to<std::string>(obj.at("update_timestamp"));
    config.config_json = boost::json::value_to<std::string>(obj.at("config_json"));
    return config;
}