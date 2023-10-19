#ifndef k2eg_CONTROLLER_NODE_CONFIGURATION_NODECONFIGURATION_H_
#define k2eg_CONTROLLER_NODE_CONFIGURATION_NODECONFIGURATION_H_

#include <k2eg/service/data/DataStorage.h>
#include "k2eg/common/types.h"
#include <memory>


namespace k2eg::controller::node::configuration {
    typedef std::vector<k2eg::service::data::repository::ChannelMonitorType> ChannelMonitorTypeConstVector;
    typedef std::function<void(const k2eg::service::data::repository::ChannelMonitorType&, int&)> ChannelMonitorTypePurgeHandler;
/**
 * Abstract the node configuration
 */
class NodeConfiguration {
    const k2eg::service::data::DataStorageShrdPtr data_storage;

public:
    NodeConfiguration(k2eg::service::data::DataStorageShrdPtr data_storage);
    NodeConfiguration() = delete;
    NodeConfiguration(const NodeConfiguration&) = delete;
    NodeConfiguration& operator=(const NodeConfiguration&) = delete;
    ~NodeConfiguration() = default;
    /**
     * Add a monitor configuration for a determinated channel for a destination topic
     return for each monitor type if the record has been inserted or not
     */
    std::vector<bool> addChannelMonitor(const ChannelMonitorTypeConstVector& channel_descriptions);
    void removeChannelMonitor(const ChannelMonitorTypeConstVector& channel_descriptions);
    size_t iterateAllChannelMonitor(size_t element_to_process, ChannelMonitorTypePurgeHandler handle);
    void resetAllChannelMonitorCheck();
};
DEFINE_PTR_TYPES(NodeConfiguration)
} // namespace k2eg::controller::node::configuration

#endif // k2eg_CONTROLLER_NODE_CONFIGURATION_NODECONFIGURATION_H_