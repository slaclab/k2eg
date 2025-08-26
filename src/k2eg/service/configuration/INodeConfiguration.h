#ifndef K2EG_SERVICE_CONFIGURATION_INODECONFIGURATION_H_
#define K2EG_SERVICE_CONFIGURATION_INODECONFIGURATION_H_

#include <k2eg/common/types.h>

#include <boost/json.hpp>

#include <cstdint>
#include <stdint.h>
#include <string>

/**
 * @namespace k2eg::service::configuration
 * @brief Contains types and interfaces for node configuration management in K2EG.
 */
namespace k2eg::service::configuration {

/**
 * @brief Configuration for the configuration service.
 *
 * Holds connection and startup options for the configuration server.
 */
struct ConfigurationServiceConfig
{
    /** Hostname or IP address of the configuration server. */
    const std::string config_server_host;
    /** Port number of the configuration server. */
    const std::int16_t config_server_port;
    /** If true, resets the configuration on node startup. */
    const bool reset_on_start;
};
DEFINE_PTR_TYPES(ConfigurationServiceConfig)

/**
 * @brief Information about serialization and destination topic for a single PV.
 */
typedef struct
{
    /** Destination topic for the PV. */
    const std::string pv_destination_topic;
    /** Serialization type for PV events. */
    const std::uint8_t event_serialization;
} PVMonitorInfo;
DEFINE_PTR_TYPES(PVMonitorInfo)

DEFINE_MMAP_FOR_TYPE(std::string, PVMonitorInfoShrdPtr, PVMonitorInfoMap)
struct NodeConfiguration;
DEFINE_PTR_TYPES(NodeConfiguration)

/**
 * @brief Status of a snapshot archiving process.
 */
enum class ArchiveStatus
{
    STOPPED = 0,            /**< Archiving is stopped. */
    SUBMITTED = 1,          /**< Archiving has been submitted. */
    PREPARE_TO_ARCHIVE = 2, /**< Preparing to archive. */
    ARCHIVING = 3,          /**< Archiving is in progress. */
    ERROR = 4               /**< Archiving encountered an error. */
};

/**
 * @brief Information about the archiving status of a node.
 *
 * Includes status, timestamps, and error messages for archiving operations.
 */
struct ArchiveStatusInfo
{
    ArchiveStatus status = ArchiveStatus::STOPPED; /**< Current archiving status. */
    std::string   topic_name;                      /**< Name of the topic being archived. */
    std::string   started_at;                      /**< ISO8601 UTC timestamp when archiving started. */
    std::string   updated_at;                      /**< ISO8601 UTC timestamp of last heartbeat/update. */
    std::string   error_message;                   /**< Error message if status is ERROR. */
};

/**
 * @brief Convert ArchiveStatus to string representation.
 * @param s ArchiveStatus to convert.
 * @return String representation of the ArchiveStatus.
 */
inline std::string toStateString(const ArchiveStatus& s)
{
    using k2eg::service::configuration::ArchiveStatus;
    switch (s)
    {
    case ArchiveStatus::STOPPED:
        return "STOPPED";
    case ArchiveStatus::PREPARE_TO_ARCHIVE:
        return "PREPARE_TO_ARCHIVE";
    case ArchiveStatus::ARCHIVING:
        return "ARCHIVING";
    case ArchiveStatus::ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

/**
 * @brief Cluster node configuration.
 *
 * Stores monitored PVs and provides methods for configuration management and serialization.
 */
struct NodeConfiguration
{
    /** Map of monitored PV names to their info for this node. */
    PVMonitorInfoMap pv_monitor_info_map;

    /**
     * @brief Check if a PV name is present in the configuration.
     * @param pv_nam PV name to check.
     * @param info PVMonitorInfo to compare.
     * @return True if present, false otherwise.
     */
    bool isPresent(const std::string& pv_nam, const PVMonitorInfo& info) const;

    /**
     * @brief Remove a PV from the configuration by key.
     * @param key Key to remove.
     * @param info PVMonitorInfo to match.
     */
    void removeFromKey(const std::string& key, const PVMonitorInfo& info);

    /**
     * @brief Serialize configuration to JSON string.
     * @param config NodeConfiguration to serialize.
     * @return JSON string.
     */
    static std::string toJson(const NodeConfiguration& config);

    /**
     * @brief Deserialize configuration from JSON string.
     * @param json_str JSON string.
     * @return NodeConfiguration object.
     */
    static NodeConfiguration fromJson(const std::string& json_str);
};

/**
 * @brief Configuration for a snapshot.
 *
 * Contains scheduling, metadata, and JSON-encoded execution details.
 */
struct SnapshotConfiguration
{
    int         weight = 0;       /**< Priority weight for scheduling. */
    std::string weight_unit;      /**< Unit of the weight (e.g., "eps", "mbps"). */
    std::string update_timestamp; /**< ISO8601 UTC timestamp of last update. */
    std::string config_json;      /**< JSON-encoded configuration for snapshot execution. */

    /**
     * @brief Serialize snapshot configuration to JSON string.
     * @param config SnapshotConfiguration to serialize.
     * @return JSON string.
     */
    static std::string toJson(const SnapshotConfiguration& config);

    /**
     * @brief Deserialize snapshot configuration from JSON string.
     * @param json_str JSON string.
     * @return SnapshotConfiguration object.
     */
    static SnapshotConfiguration fromJson(const std::string& json_str);
};

DEFINE_PTR_TYPES(SnapshotConfiguration);

/*
The INodeConfiguration interface defines the base logic for node configuration services.
*/

/**
 * @brief Interface for node configuration services.
 *
 * Provides methods for managing node and snapshot configurations, archiving status, and distributed snapshot control.
 */
class INodeConfiguration
{
protected:
    /** Configuration service settings for this node. */
    ConstConfigurationServiceConfigUPtr config;

public:
    /**
     * @brief Construct a new INodeConfiguration.
     * @param config Configuration service settings.
     */
    INodeConfiguration(ConstConfigurationServiceConfigUPtr config);

    /**
     * @brief Destroy the INodeConfiguration object.
     */
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
    virtual void setSnapshotArchiveStatus(const std::string& snapshot_id, const ArchiveStatusInfo& status) = 0;

    /**
     * @brief Get the archiving status of a snapshot.
     * @param snapshot_id ID of the snapshot to check.
     * @return Shared pointer to the ArchiveStatusInfo object containing status information.
     */
    virtual ArchiveStatusInfo getSnapshotArchiveStatus(const std::string& snapshot_id) const = 0;

    /**
     * @brief Set the statistics for a snapshot.
     * @param snapshot_id ID of the snapshot to update.
     * @param stat_value New statistics value.
     * @param stat_type Type of the statistics being set.
     */
    virtual void setSnapshotWeight(const std::string& snapshot_id, const std::string& weight, const std::string& weight_unit) = 0;

    /**
     * @brief Get the gateway ID that is currently managing a snapshot.
     * @param snapshot_id ID of the snapshot to check.
     * @return Gateway ID if the snapshot is running, empty string otherwise.
     */
    virtual const std::string getSnapshotGateway(const std::string& snapshot_id) const = 0;

    /**
     * @brief Get the ID of the archiver node for a snapshot.
     * @param snapshot_id ID of the snapshot to check.
     * @return Archiver node ID if the snapshot is archived, empty string otherwise.
     */
    virtual const std::string getSnapshotArchiver(const std::string& snapshot_id) const = 0;

    /**
     * @brief Try to acquire a snapshot for the current node.
     * @param snapshot_id ID of the snapshot to acquire.
     * @param for_gateway True if the acquisition is for a gateway, for storage otherwise.
     * @details This method attempts to acquire a snapshot for the current node, allowing it to manage the snapshot.
     * If the acquire is for gateway, it also sets the running status to true.
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
     * @brief Get the IDs of snapshots available for execution.
     * @details Checks for snapshot configurations eligible to be started or resumed, typically after a crash or unexpected shutdown.
     * Returns IDs of snapshots not currently locked or managed by another node.
     * @return Vector of available snapshot IDs, or empty if none are available.
     */
    virtual const std::vector<std::string> getAvailableSnapshot() const = 0;

    /**
     * @brief Get the list of running snapshots that are requested to be archived.
     * @return Vector of snapshot IDs that are running and requested for archiving.
     */
    virtual const std::vector<std::string> getRunningSnapshotToArchive() const = 0;
};
DEFINE_PTR_TYPES(INodeConfiguration)
} // namespace k2eg::service::configuration

#endif // K2EG_SERVICE_CONFIGURATION_INODECONFIGURATION_H_