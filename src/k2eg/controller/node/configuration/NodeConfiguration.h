#ifndef k2eg_CONTROLLER_NODE_CONFIGURATION_NODECONFIGURATION_H_
#define k2eg_CONTROLLER_NODE_CONFIGURATION_NODECONFIGURATION_H_

#include <k2eg/service/data/DataStorage.h>

#include <memory>

namespace k2eg::controller::node::configuration {
    typedef std::vector<k2eg::service::data::repository::PVMonitorType> PVMonitorTypeConstVector;
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
     * Add a monitor configuration for a determinated pv for a destination topic
     */
    void addPVMonitor(const PVMonitorTypeConstVector& pv_descriptions);
    void removePVMonitor(const PVMonitorTypeConstVector& pv_descriptions);
    void iterateAllPVMonitor(k2eg::service::data::repository::PVMonitorTypeProcessHandler handle);
};

typedef std::unique_ptr<NodeConfiguration> NodeConfigurationUPtr;
} // namespace k2eg::controller::node::configuration

#endif // k2eg_CONTROLLER_NODE_CONFIGURATION_NODECONFIGURATION_H_