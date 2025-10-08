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
 * @brief Facade over node configuration storage and helpers.
 *
 * Loads configuration, manages monitor entries, and exposes iteration helpers
 * used by workers. Backed by shared DataStorage and configuration service.
 */
class NodeConfiguration {
    const k2eg::service::data::DataStorageShrdPtr data_storage;
    k2eg::service::configuration::NodeConfigurationShrdPtr node_configuration;
    k2eg::service::configuration::INodeConfigurationShrdPtr node_configuration_service;
public:
    /**
     * @brief Construct with backing data storage.
     */
    NodeConfiguration(k2eg::service::data::DataStorageShrdPtr data_storage);
    NodeConfiguration() = delete;
    NodeConfiguration(const NodeConfiguration&) = delete;
    NodeConfiguration& operator=(const NodeConfiguration&) = delete;
    ~NodeConfiguration() = default;

    /**
     * @brief Load node configuration from the cluster configuration service.
     */
    void loadNodeConfiguration();

    /**
     * @brief Add monitor configurations for channels and destination topics.
     * @param channel_descriptions List of channel monitor descriptors to add.
     * @return For each descriptor, whether a new record was inserted.
     */
    std::vector<bool> addChannelMonitor(const ChannelMonitorTypeConstVector& channel_descriptions);

    /**
     * @brief Remove monitor configurations for the given channels/topics.
     */
    void removeChannelMonitor(const ChannelMonitorTypeConstVector& channel_descriptions);

    /** @brief Iterate a fixed number of monitor records applying a handler. */
    size_t iterateAllChannelMonitor(size_t element_to_process, ChannelMonitorTypeHandler handle);

    /**
     * @brief Iterate over registered monitors to evaluate stop conditions.
     */
    size_t iterateAllChannelMonitorForAction(size_t element_to_process, ChannelMonitorTypeHandler handle);

    /** @brief Reset monitoring checks for all channels. */
    void resetAllChannelMonitorCheck();

    /** @brief Return current node name. */
    const std::string getNodeName() const;
};
DEFINE_PTR_TYPES(NodeConfiguration)
} // namespace k2eg::controller::node::configuration

#endif // k2eg_CONTROLLER_NODE_CONFIGURATION_NODECONFIGURATION_H_
