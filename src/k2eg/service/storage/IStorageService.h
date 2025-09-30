#ifndef K2EG_SERVICE_STORAGE_ISTORAGESERVICE_H_
#define K2EG_SERVICE_STORAGE_ISTORAGESERVICE_H_

#include <k2eg/common/BaseSerialization.h>
#include <k2eg/common/types.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace k2eg::service::storage {

// StoredData is a concrete implementation of the Data interface for data
// retrieved from persistent storage. It provides a non-owning view over
// memory managed by this container instance. Use via StoredMessage::data().
/**
 * @brief Non-owning view over stored bytes.
 */
class StoredData : public common::Data
{
    const char* data_{nullptr};
    size_t      size_{0};

public:
    // Non-owning view over external memory. Caller guarantees lifetime.
    StoredData(const char* data, size_t size)
        : data_(data), size_(size) {}

    virtual ~StoredData() = default;

    const size_t size() const override
    {
        return size_;
    }

    const char* data() const override
    {
        return data_;
    }
};

// StoredMessage is a concrete implementation of the SerializedMessage
// interface, designed to hold data retrieved from storage. It owns an
// internal buffer and exposes a ConstDataUPtr view each time data() is
// called. The associated serialization type (e.g. MsgPack) is tracked.
/**
 * @brief Owned buffer for a stored message.
 */
class StoredMessage : public common::SerializedMessage
{
    std::vector<char>         buffer;
    common::SerializationType serialization_type_{common::SerializationType::Unknown};

public:
    // Constructor that takes a vector of chars and moves it into the buffer.
    explicit StoredMessage(std::vector<char>&& data, common::SerializationType ser_type = common::SerializationType::Unknown)
        : buffer(std::move(data)), serialization_type_(ser_type) {}

    // Constructor that copies a chunk of memory into the buffer.
    StoredMessage(const char* data, size_t size, common::SerializationType ser_type = common::SerializationType::Unknown)
        : buffer(data, data + size), serialization_type_(ser_type) {}

    virtual ~StoredMessage() = default;

    common::ConstDataUPtr
    data() const override
    {
        return std::make_unique<StoredData>(buffer.data(), buffer.size());
    }

    // Return the serialization type associated with this message
    common::SerializationType
    serializationType() const
    {
        return serialization_type_;
    }
};
DEFINE_PTR_TYPES(StoredMessage)

/**
 * @brief Snapshot of PV data at a specific time
 *
 * A snapshot is a named, time-stamped grouping of PV names that can be
 * referenced during queries to retrieve the point-in-time view of data.
 * Implementations should ensure the pair (snapshot_id, pv_name) is
 * efficiently queryable. The optional search_key can be used for
 * human-friendly lookup or composite identification.
 */
/**
 * @brief Snapshot of PV data at a specific time.
 */
struct Snapshot
{
    std::string                           snapshot_id;   ///< Unique identifier for the snapshot
    std::string                           snapshot_name; ///< Human-readable name for the snapshot
    std::chrono::system_clock::time_point created_at;    ///< Timestamp when the snapshot was created
    std::string                           search_key;    ///< Format: "snapshot_name:header_timestamp:iter_index"
};
DEFINE_PTR_TYPES(Snapshot)

/**
 * @brief A single archived record
 *
 * Each record captures a PV point with optional topic partitioning,
 * metadata and payload. Implementations should avoid duplicates for
 * the tuple (pv_name, timestamp, topic, snapshot_id) and may upsert.
 */
/**
 * @brief A single archived record.
 */
struct ArchiveRecord
{
    std::string                           pv_name;
    std::string                           topic;
    std::chrono::system_clock::time_point timestamp;
    std::string                           metadata;
    ConstStoredMessageUPtr                data;
    std::optional<std::string>            snapshot_id; // Optional snapshot identifier
};

/**
 * @brief Query for archived data
 *
 * Provide any subset of the fields to constrain the result set.
 * If both start_time and end_time are provided, the interval is closed
 * [start_time, end_time]. If limit is provided, implementations should
 * return at most that many records and indicate via has_more if more exist.
 */
/**
 * @brief Query for archived data.
 */
struct ArchiveQuery
{
    std::string                                          pv_name;
    std::optional<std::string>                           topic;
    std::optional<std::chrono::system_clock::time_point> start_time;
    std::optional<std::chrono::system_clock::time_point> end_time;
    std::optional<size_t>                                limit;
    std::optional<std::string>                           snapshot_id; // Optional snapshot filter
};

/**
 * @brief Result of an archive query
 */
/**
 * @brief Result of an archive query.
 */
struct ArchiveQueryResult
{
    std::vector<ArchiveRecord> records;
    size_t                     total_count = 0;
    bool                       has_more = false;
};

/**
 * @brief Result of a snapshot ID range query.
 */
struct SnapshotIdRangeResult
{
    std::vector<std::string>   snapshot_ids;
    bool                       has_more = false;
    std::optional<std::string> continuation_token; // last returned ID
};
DEFINE_PTR_TYPES(SnapshotIdRangeResult)

/**
 * @brief Base configuration for storage implementations
 *
 * Concrete backends should derive from this type to expose their
 * configuration through a shared base pointer.
 */
/**
 * @brief Base configuration for storage backends.
 */
struct StorageImplementationConfig
{
};
DEFINE_PTR_TYPES(StorageImplementationConfig)

/**
 * @brief Interface for storage services
 *
 * Contract for services that persist and retrieve EPICS data. It includes
 * CRUD-like operations for records, query APIs, health checks, and snapshot
 * management. Implementations must be thread-safe where appropriate.
 */
/**
 * @brief Contract for EPICS data storage services.
 */
class IStorageService
{
public:
    virtual ~IStorageService() = default;
    /**
     * @brief Store a single record.
     * @param record Archived record to persist.
     * @return True on success; false on failure.
     */
    virtual bool store(const ArchiveRecord& record) = 0;
    /**
     * @brief Store a batch of records.
     * @param records Records to persist.
     * @return Number of records successfully stored.
     */
    virtual size_t storeBatch(const std::vector<ArchiveRecord>& records) = 0;
    /**
     * @brief Query archived records.
     * @param query Constraints such as pv_name, time range, limit.
     * @return Matching records plus paging metadata.
     */
    virtual ArchiveQueryResult query(const ArchiveQuery& query) = 0;
    /**
     * @brief Report backend health.
     * @return True if backend is reachable/operational.
     */
    virtual bool isHealthy() = 0;
    /**
     * @brief Clear all persisted data (tests).
     * @return True on successful deletion; false on error.
     */
    virtual bool clearAllData() = 0;
    /**
     * @brief Create a new snapshot entry.
     * @param snapshot Snapshot metadata (name, description, pv_names, etc.).
     * @return Unique snapshot identifier. May be generated by implementation.
     */
    virtual std::string createSnapshot(const Snapshot& snapshot) = 0;
    /**
     * @brief Delete a snapshot by id.
     * @param snapshot_id Unique snapshot identifier.
     * @return True if a snapshot was removed; false if not found.
     */
    virtual bool deleteSnapshot(const std::string& snapshot_id) = 0;
    /**
     * @brief List available snapshots.
     * @return Snapshots, typically sorted by creation time descending.
     */
    virtual std::vector<Snapshot> listSnapshots() = 0;
    /**
     * @brief Get a snapshot by id.
     * @param snapshot_id Unique snapshot identifier.
     * @return Snapshot if found; std::nullopt otherwise.
     */
    virtual std::optional<Snapshot> getSnapshot(const std::string& snapshot_id) = 0;
    /**
     * @brief Find snapshot by search key.
     * @param search_key Unique, human/composite search key.
     * @return Snapshot if matched; std::nullopt otherwise.
     */
    virtual std::optional<Snapshot> findSnapshotBySearchKey(const std::string& search_key) = 0;

    /**
     * @brief List snapshot IDs in a time range with pagination.
     * @param start_time Inclusive start time.
     * @param end_time Inclusive end time.
     * @param limit Maximum number of IDs to return.
     * @param continuation_token Optional last seen snapshot ID to continue after.
     * @return Snapshot IDs plus paging metadata and next continuation token.
     */
    virtual SnapshotIdRangeResult listSnapshotIdsInRange(
        const std::chrono::system_clock::time_point& start_time,
        const std::chrono::system_clock::time_point& end_time,
        size_t                                       limit,
        const std::optional<std::string>&            continuation_token = std::nullopt) = 0;
};

DEFINE_PTR_TYPES(IStorageService)

} // namespace k2eg::service::storage

#endif // K2EG_SERVICE_STORAGE_ISTORAGESERVICE_H_
