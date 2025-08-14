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

enum class ArchiveStatus
{
    STOPPED = 0,
    ARCHIVING = 1,
    ERROR = 2
};

struct ArchiveStatusInfo
{
    ArchiveStatus status = ArchiveStatus::STOPPED;
    std::string   started_at; // ISO8601 UTC
    std::string   updated_at; // ISO8601 UTC (heartbeat)
    std::string   error_message;
};

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

    static std::string       toJson(const NodeConfiguration& config);
    static NodeConfiguration fromJson(const std::string& json_str);
};

// Snapshot configuration structure
struct SnapshotConfiguration
{
    // Priority weight of the snapshot, used for scheduling
    int weight = 0;

    // Unit of the weight, e.g., "eps" (events/sec) or "mbps" (megabits/sec)
    std::string weight_unit;

    // ISO 8601 UTC timestamp (e.g., "2025-07-29T10:15:30Z") of snapshot update
    std::string update_timestamp;

    // JSON-encoded configuration for snapshot execution (e.g., PV list, interval)
    std::string                  config_json;
    static std::string           toJson(const SnapshotConfiguration& config);
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
    INodeConfiguration(ConstConfigurationServiceConfigUPtr config);
    virtual ~INodeConfiguration();
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
     * @brief Check if a snapshot is marked for archiving.
     * @param snapshot_id ID of the snapshot to check.
     * @return True if the snapshot is marked for archiving, false otherwise.
     */
    virtual bool isSnapshotArchiveRequested(const std::string& snapshot_id) const = 0;
    /**
     * @brief Set the archiving status of a snapshot.
     * @param snapshot_id ID of the snapshot to update.
     * @param archived True if the snapshot is marked for archiving, false otherwise.
     */
    virtual void setSnapshotArchiveRequested(const std::string& snapshot_id, bool archived) = 0;
    /**
     * @brief Set the archiving status of a snapshot.
     * @param snapshot_id ID of the snapshot to update.
     * @param status New archiving status information.
     */
    virtual void setSnapshotArchiveStatus(const std::string& snapshot_id, ArchiveStatusInfo status) = 0;
    /**
     * @brief Get the archiving status of a snapshot.
     * @param snapshot_id ID of the snapshot to check.
     * @return Archiving status information.
     */
    virtual ArchiveStatusInfo getSnapshotArchiveStatus(const std::string& snapshot_id) const = 0;
    /**
     * @brief Get the gateway ID that is currently managing a snapshot.
     * @param snapshot_id ID of the snapshot to check.
     * @return Gateway ID if the snapshot is running, empty string otherwise.
     */
    virtual const std::string getSnapshotGateway(const std::string& snapshot_id) const = 0;
    /**
     * @brief Try to acquire a snapshot for the current node.
     * @param snapshot_id ID of the snapshot to acquire.
     * @param for_gateway True if the acquisition is for a gateway, for storage otherwise.
     * @details This method attempts to acquire a snapshot for the current node, allowing it to manage the snapshot.
     * if the acquire is for gateway in the same time it set the running status to true
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
    /**
     * @brief Get the ID of a snapshot that is available for execution.
     * @details This method checks for a snapshot configuration that is eligible to be started or resumed.
     * It is typically used to detect snapshots that remain in a "running" state due to an unexpected node shutdown,
     * crash, or reboot, where the snapshot was not explicitly stopped and its distributed lock/session has expired.
     * Implementations should return the ID of a snapshot that is not currently locked or managed by another node,
     * allowing recovery or continuation of unfinished snapshot tasks.
     * If no such snapshot exists, an empty string is returned.
     * @return The ID of the available snapshot as a string, or an empty string if none are available.
     */
    virtual const std::vector<std::string> getAvailableSnapshot() const = 0;
};
DEFINE_PTR_TYPES(INodeConfiguration)
} // namespace k2eg::service::configuration

#endif // K2EG_SERVICE_CONFIGURATION_INODECONFIGURATION_H_