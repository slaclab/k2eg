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

namespace k2eg::service::storage {

// StoredData is a concrete implementation of the Data interface, designed to hold data retrieved from storage.
class StoredData : public common::Data
{
    std::vector<char> buffer;

public:
    // Constructor that takes a vector of chars and moves it into the buffer.
    explicit StoredData(std::vector<char>&& data)
        : buffer(std::move(data)) {}

    // Constructor that copies a chunk of memory into the buffer.
    StoredData(const char* data, size_t size)
        : buffer(data, data + size) {}

    virtual ~StoredData() = default;

    // Returns the size of the stored data.
    const size_t
    size() const override
    {
        return buffer.size();
    }

    // Returns a pointer to the stored data.
    const char*
    data() const override
    {
        return buffer.data();
    }
};

// StoredMessage is a concrete implementation of the SerializedMessage interface, designed to hold data retrieved from storage.
class StoredMessage : public common::SerializedMessage
{
    std::vector<char> buffer;

public:
    // Constructor that takes a vector of chars and moves it into the buffer.
    explicit StoredMessage(std::vector<char>&& data)
        : buffer(std::move(data)) {}

    // Constructor that copies a chunk of memory into the buffer.
    StoredMessage(const char* data, size_t size)
        : buffer(data, data + size) {}

    virtual ~StoredMessage() = default;

    common::ConstDataUPtr
    data() const override
    {
        return std::make_unique<StoredData>(buffer.data(), buffer.size());
    }
};
DEFINE_PTR_TYPES(StoredMessage)

/**
 * @brief Represents a record to be archived
 */
struct ArchiveRecord
{
    std::string                           pv_name;
    std::string                           topic;
    std::chrono::system_clock::time_point timestamp;
    uint64_t                              message_id;
    std::string                           metadata;
    common::ConstSerializedMessageUPtr    data;
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
};

DEFINE_PTR_TYPES(IStorageService)

} // namespace k2eg::service::storage

#endif // K2EG_SERVICE_STORAGE_ISTORAGESERVICE_H_