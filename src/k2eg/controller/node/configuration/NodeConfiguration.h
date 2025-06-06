#ifndef k2eg_CONTROLLER_NODE_CONFIGURATION_NODECONFIGURATION_H_
#define k2eg_CONTROLLER_NODE_CONFIGURATION_NODECONFIGURATION_H_


#include <k2eg/common/types.h>

#include <k2eg/service/data/DataStorage.h>
#include <k2eg/service/configuration/INodeConfiguration.h>
#include <string>

namespace k2eg::controller::node::configuration {
    typedef std::vector<k2eg::service::data::repository::ChannelMonitorType> ChannelMonitorTypeConstVector;
    typedef std::function<void(const k2eg::service::data::repository::ChannelMonitorType&, int&)> ChannelMonitorTypeHandler;

/**
 * Abstract the node configuration
 */
class NodeConfiguration {
    const k2eg::service::data::DataStorageShrdPtr data_storage;
    k2eg::service::configuration::NodeConfigurationShrdPtr node_configuration;
    k2eg::service::configuration::INodeConfigurationShrdPtr node_configuration_service;
public:
    NodeConfiguration(k2eg::service::data::DataStorageShrdPtr data_storage);
    NodeConfiguration() = delete;
    NodeConfiguration(const NodeConfiguration&) = delete;
    NodeConfiguration& operator=(const NodeConfiguration&) = delete;
    ~NodeConfiguration() = default;

    /**
     * Load the node configuration from the global shared
     * cluster configuration management
     */
    void loadNodeConfiguration();

    /**
     * Add a monitor configuration for a determinated channel for a destination topic
     return for each monitor type if the record has been inserted or not
     */
    std::vector<bool> addChannelMonitor(const ChannelMonitorTypeConstVector& channel_descriptions);

    /**
     * Remove a monitor configuration for a determinated channel for a destination topic
     */
    void removeChannelMonitor(const ChannelMonitorTypeConstVector& channel_descriptions);

    // iterate over monitor for general purphose logic
    size_t iterateAllChannelMonitor(size_t element_to_process, ChannelMonitorTypeHandler handle);

    // interato over all registered monitor for manage the stop of these that respect some logic drivern
    // by the handler
    size_t iterateAllChannelMonitorForAction(size_t element_to_process, ChannelMonitorTypeHandler handle);

    // reset all channel monitor check
    void resetAllChannelMonitorCheck();

    const std::string getNodeName() const;
};
DEFINE_PTR_TYPES(NodeConfiguration)
} // namespace k2eg::controller::node::configuration

#endif // k2eg_CONTROLLER_NODE_CONFIGURATION_NODECONFIGURATION_H_