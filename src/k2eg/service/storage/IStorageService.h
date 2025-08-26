#ifndef K2EG_SERVICE_STORAGE_ISTORAGESERVICE_H_
#define K2EG_SERVICE_STORAGE_ISTORAGESERVICE_H_

#include <k2eg/common/BaseSerialization.h>
#include <k2eg/common/types.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <unordered_set>

namespace k2eg::service::storage {

// StoredData is a concrete implementation of the Data interface, designed to hold data retrieved from storage.
class StoredData : public common::Data
{
    const char* data_{nullptr};
    size_t      size_{0};

public:
    // Non-owning view over external memory. Caller guarantees lifetime.
    StoredData(const char* data, size_t size) : data_(data), size_(size) {}
    virtual ~StoredData() = default;

    const size_t size() const override { return size_; }
    const char*  data() const override { return data_; }
};

// StoredMessage is a concrete implementation of the SerializedMessage interface, designed to hold data retrieved from storage.
class StoredMessage : public common::SerializedMessage
{
    std::vector<char>            buffer;
    common::SerializationType    serialization_type_ {common::SerializationType::Unknown};

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
    serializationType() const { return serialization_type_; }
};
DEFINE_PTR_TYPES(StoredMessage)

/**
 * @brief Represents a snapshot of PV data at a specific time
 */
struct Snapshot
{
    std::string                           snapshot_id;
    std::string                           snapshot_name;
    std::chrono::system_clock::time_point created_at;
    std::string                           description;
    std::unordered_set<std::string>       pv_names;
};
DEFINE_PTR_TYPES(Snapshot)

/**
 * @brief Represents a record to be archived
 */
struct ArchiveRecord
{
    std::string                           pv_name;
    std::string                           topic;
    std::chrono::system_clock::time_point timestamp;
    std::string                           metadata;
    ConstStoredMessageUPtr                data;
    std::optional<std::string>            snapshot_id;  // Optional snapshot identifier
};

/**
 * @brief Represents a query for archived data
 */
struct ArchiveQuery
{
    std::string                                          pv_name;
    std::optional<std::string>                           topic;
    std::optional<std::chrono::system_clock::time_point> start_time;
    std::optional<std::chrono::system_clock::time_point> end_time;
    std::optional<size_t>                                limit;
    std::optional<std::string>                           snapshot_id;  // Optional snapshot filter
};

/**
 * @brief Represents the result of an archive query
 */
struct ArchiveQueryResult
{
    std::vector<ArchiveRecord> records;
    size_t                     total_count = 0;
    bool                       has_more = false;
};

/**
 * @brief Base configuration for storage implementations
 */
struct StorageImplementationConfig
{
};
DEFINE_PTR_TYPES(StorageImplementationConfig)

/**
 * @brief Interface for storage services
 *
 * This interface defines the contract for services that provide storage
 * for EPICS data, including methods for storing and retrieving records.
 */
class IStorageService
{
public:
    virtual ~IStorageService() = default;
    virtual bool               store(const ArchiveRecord& record) = 0;
    virtual size_t             storeBatch(const std::vector<ArchiveRecord>& records) = 0;
    virtual ArchiveQueryResult query(const ArchiveQuery& query) = 0;
    virtual bool               isHealthy() = 0;
    
    // Snapshot management methods
    virtual std::string        createSnapshot(const Snapshot& snapshot) = 0;
    virtual bool               deleteSnapshot(const std::string& snapshot_id) = 0;
    virtual std::vector<Snapshot> listSnapshots() = 0;
    virtual std::optional<Snapshot> getSnapshot(const std::string& snapshot_id) = 0;
};

DEFINE_PTR_TYPES(IStorageService)

} // namespace k2eg::service::storage

#endif // K2EG_SERVICE_STORAGE_ISTORAGESERVICE_H_
