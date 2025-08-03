#ifndef K2EG_SERVICE_CONFIGURATION_INODECONFIGURATION_H_
#define K2EG_SERVICE_CONFIGURATION_INODECONFIGURATION_H_

#include <k2eg/common/types.h>

#include <boost/json.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <stdint.h>

namespace k2eg::service::configuration {

/*
is the configuration service config
*/
struct ConfigurationServiceConfig
{
    const std::string  config_server_host;
    const std::int16_t config_server_port;
    // reset the configration on the start of the node
    const bool reset_on_start;
};
DEFINE_PTR_TYPES(ConfigurationServiceConfig)

/// The informazion about serialziation and destination topic for a single PV
typedef struct
{
    // serialization type
    const std::string pv_destination_topic;
    // destination topic
    const std::uint8_t event_serialization;
} PVMonitorInfo;
DEFINE_PTR_TYPES(PVMonitorInfo)

DEFINE_MMAP_FOR_TYPE(std::string, PVMonitorInfoShrdPtr, PVMonitorInfoMap)
struct NodeConfiguration;
DEFINE_PTR_TYPES(NodeConfiguration)

/*
Is the cluster node configuration
*/
struct NodeConfiguration
{
    // the list of monitored PV names for the single node
    PVMonitorInfoMap pv_monitor_info_map;

    // check if the new PV name is already present for the specific configuration
    bool isPresent(const std::string& pv_nam, const PVMonitorInfo& info) const;

    void removeFromKey(const std::string& key, const PVMonitorInfo& info);

    static std::string toJson(const NodeConfiguration& config);
    static NodeConfiguration fromJson(const std::string& json_str);
};

// Snapshot configuration structure
struct SnapshotConfiguration
{
    // Priority weight of the snapshot, used for scheduling
    int weight = 0;

    // Unit of the weight, e.g., "eps" (events/sec) or "mbps" (megabits/sec)
    std::string weight_unit;

    // ID of the gateway that created and is managing the snapshot
    std::string gateway_id;

    // True if the snapshot is currently active on the gateway (session-bound)
    bool running_status = false;

    // True if archiving is enabled for this snapshot
    bool archiving_status = false;

    // ID of the archiver currently responsible for storing this snapshot
    std::string archiver_id;

    // ISO 8601 UTC timestamp (e.g., "2025-07-29T10:15:30Z") of snapshot update
    std::string update_timestamp;

    // JSON-encoded configuration for snapshot execution (e.g., PV list, interval)
    std::string config_json;
    static std::string toJson(const SnapshotConfiguration& config);
    static SnapshotConfiguration fromJson(const std::string& json_str);
};

DEFINE_PTR_TYPES(SnapshotConfiguration);

/*
The INodeConfiguration interface defines the base logic for node configuration services.
*/
class INodeConfiguration
{
protected:
    ConstConfigurationServiceConfigUPtr config;

public:
    INodeConfiguration(ConstConfigurationServiceConfigUPtr config)
        : config(std::move(config)){};
    virtual ~INodeConfiguration() = default;
    /**
     * @brief Get the node configuration.
     * @return Shared pointer to the node configuration.
     */
    virtual NodeConfigurationShrdPtr getNodeConfiguration() const = 0;
    /**
     * @brief Set the node configuration.
     * @param node_configuration Shared pointer to the node configuration to set.
     * @return True if the configuration was successfully set, false otherwise.
     */
    virtual bool setNodeConfiguration(NodeConfigurationShrdPtr node_configuration) = 0;
    /**
     * @brief Get the name of the node.
     * @return The name of the node as a string.
     */
    virtual std::string getNodeName() const = 0;

    /**
     * @brief Get the key for a specific snapshot by its ID.
     * @param snapshot_id ID of the snapshot to retrieve the key for.
     * @return The key string for the snapshot.
     */
    virtual const std::string getSnapshotKey(const std::string& snapshot_id) const = 0;
    /**
     * @brief Get the configuration for a specific snapshot by its ID.
     * @param snapshot_id ID of the snapshot to retrieve.
     * @return Shared pointer to the snapshot configuration, or nullptr if not found.
     */
    virtual ConstSnapshotConfigurationShrdPtr getSnapshotConfiguration(const std::string& snapshot_id) const = 0;
    /**
     * @brief Set the configuration for a specific snapshot.
     * @param snapshot_id ID of the snapshot to configure.
     * @param snapshot_config Configuration object containing snapshot settings.
     * @return True if the configuration was successfully set, false otherwise.
     */
    virtual bool setSnapshotConfiguration(const std::string& snapshot_id, SnapshotConfigurationShrdPtr snapshot_config) = 0;
    /**
     * @brief Delete a snapshot configuration by its ID.
     * @param snapshot_id ID of the snapshot to delete.
     * @return True if the deletion was successful, false otherwise.
     */
    virtual bool deleteSnapshotConfiguration(const std::string& snapshot_id) = 0;
    /**
     * @brief Get the list of snapshot IDs managed by the node.
     * @return Vector of snapshot IDs.
     */
    virtual const std::vector<std::string> getSnapshotIds() const = 0;

    /**
     * @brief Get the value of a specific field in a snapshot configuration.
     * @param snapshot_id ID of the snapshot to query.
     * @param field Name of the field to retrieve.
     * @return The value of the specified field, or an empty string if not found.
     */
    virtual const std::string getSnapshotField(const std::string& snapshot_id, const std::string& field) const = 0;

    // Distributed snapshot management methods
    /**
     * @brief Check if a snapshot is currently running on the node.
     * @param snapshot_id ID of the snapshot to check.
     * @return True if the snapshot is running, false otherwise.
     */
    virtual bool isSnapshotRunning(const std::string& snapshot_id) const = 0;

    /**
     * @brief Set the running status of a snapshot.
     * @param snapshot_id ID of the snapshot to update.
     * @param running True if the snapshot is running, false otherwise.
     */
    virtual void setSnapshotRunning(const std::string& snapshot_id, bool running) = 0;

    /**
     * @brief Get the gateway ID that is currently managing a snapshot.
     * @param snapshot_id ID of the snapshot to check.
     * @return Gateway ID if the snapshot is running, empty string otherwise.
     */
    virtual const std::string getSnapshotGateway(const std::string& snapshot_id) const = 0;
    /**
     * @brief Try to acquire a snapshot for the current node.
     * @param snapshot_id ID of the snapshot to acquire.
     * @param for_gateway True if the acquisition is for a gateway, false otherwise.
     * @return True if the snapshot was successfully acquired, false otherwise.
     */
    virtual bool tryAcquireSnapshot(const std::string& snapshot_id, bool for_gateway) = 0;
    /**
     * @brief Release a snapshot that was acquired by the current node.
     * @param snapshot_id ID of the snapshot to release.
     * @param for_gateway True if the release is for a gateway, false otherwise.
     * @return True if the snapshot was successfully released, false otherwise.
     */
    virtual bool releaseSnapshot(const std::string& snapshot_id, bool for_gateway) = 0;
    /**
     * @brief Get the list of currently running snapshots on the node.
     * @return Vector of snapshot IDs that are currently running.
     */
    virtual const std::vector<std::string> getRunningSnapshots() const = 0;
    /**
     * @brief Get the list of all snapshots managed by the node.
     * @return Vector of snapshot IDs that are managed by the node.
     */
    virtual const std::vector<std::string> getSnapshots() const = 0;
};
DEFINE_PTR_TYPES(INodeConfiguration)
} // namespace k2eg::service::configuration

#endif // K2EG_SERVICE_CONFIGURATION_INODECONFIGURATION_H_