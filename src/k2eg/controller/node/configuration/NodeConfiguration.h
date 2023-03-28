#ifndef k2eg_CONTROLLER_NODE_CONFIGURATION_NODECONFIGURATION_H_
#define k2eg_CONTROLLER_NODE_CONFIGURATION_NODECONFIGURATION_H_

#include <k2eg/service/data/DataStorage.h>

#include <memory>

namespace k2eg::controller::node::configuration {
    typedef std::vector<k2eg::service::data::repository::ChannelMonitorType> ChannelMonitorTypeConstVector;
/**
 * Abstract the node configuration
 */
class NodeConfiguration {
    const k2eg::service::data::DataStorageUPtr data_storage;

public:
    NodeConfiguration(k2eg::service::data::DataStorageUPtr data_storage);
    NodeConfiguration() = delete;
    NodeConfiguration(const NodeConfiguration&) = delete;
    NodeConfiguration& operator=(const NodeConfiguration&) = delete;
    ~NodeConfiguration() = default;
    /**
     * Add a monitor configuration for a determinated channel for a destination topic
     */
    void addChannelMonitor(const ChannelMonitorTypeConstVector& channel_descriptions);
    void removeChannelMonitor(const ChannelMonitorTypeConstVector& channel_descriptions);
    void iterateAllChannelMonitor(k2eg::service::data::repository::ChannelMonitorTypeProcessHandler handle);
};

typedef std::unique_ptr<NodeConfiguration> NodeConfigurationUPtr;
} // namespace k2eg::controller::node::configuration

#endif // k2eg_CONTROLLER_NODE_CONFIGURATION_NODECONFIGURATION_H_