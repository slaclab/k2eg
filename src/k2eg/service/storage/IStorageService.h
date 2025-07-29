#ifndef K2EG_SERVICE_STORAGE_ISTORAGESERVICE_H_
#define K2EG_SERVICE_STORAGE_ISTORAGESERVICE_H_

#include <k2eg/common/types.h>

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace k2eg::service::storage {

struct StorageImplementationConfig
{
};

DEFINE_PTR_TYPES(StorageImplementationConfig);

/**
 * @brief Enum representing the processing state of an archive record
 */
enum class ProcessingState
{
    RAW_STORED,        // Only raw data stored
    ENRICHMENT_QUEUED, // Enrichment task queued
    ENRICHED,          // Metadata extracted and stored
    ENRICHMENT_FAILED  // Enrichment failed, raw data still available
};

/**
 * @brief Simplified archive record for raw storage
 */
struct RawMsgPackRecord
{
    std::string                           pv_name;
    std::string                           topic;
    std::chrono::system_clock::time_point timestamp;
    std::vector<uint8_t>                  msgpack_data; // Raw msgpack payload
    uint64_t                              message_id;
    ProcessingState                                      processing_state = ProcessingState::RAW_STORED;
    std::chrono::system_clock::time_point                created_at;
    std::optional<std::chrono::system_clock::time_point> enriched_at;
};

/**
 * @brief Data structure for storing archived EPICS data
 */
struct ArchiveRecord
{
    std::string                           pv_name;
    std::string                           topic;
    std::chrono::system_clock::time_point timestamp;
    std::vector<uint8_t>                  data;
    std::string                           metadata; // JSON metadata
    uint64_t                              message_id;
};

/**
 * @brief Query parameters for retrieving archived data
 */
struct ArchiveQuery
{
    std::string                                          pv_name;
    std::optional<std::chrono::system_clock::time_point> start_time;
    std::optional<std::chrono::system_clock::time_point> end_time;
    std::optional<size_t>                                limit;
    std::optional<std::string>                           topic;
};

/**
 * @brief Result structure for archive queries
 */
struct ArchiveQueryResult
{
    std::vector<ArchiveRecord> records;
    size_t                     total_count;
    bool                       has_more;
};

/**
 * @brief Abstract interface for storage services
 *
 * This interface defines the contract for different storage implementations
 * (MongoDB, SQLite, etc.) used by the archiver workers.
 */
class IStorageService
{
public:
    virtual ~IStorageService() = default;

    /**
     * @brief Store a single archive record
     * @param record The record to store
     * @return true if storage was successful
     */
    virtual bool store(const ArchiveRecord& record) = 0;

    /**
     * @brief Store multiple archive records in a batch
     * @param records Vector of records to store
     * @return Number of successfully stored records
     */
    virtual size_t storeBatch(const std::vector<ArchiveRecord>& records) = 0;

    /**
     * @brief Query archived data
     * @param query Query parameters
     * @return Query result with matching records
     */
    virtual ArchiveQueryResult query(const ArchiveQuery& query) = 0;

    /**
     * @brief Check if the storage service is healthy
     * @return true if healthy
     */
    virtual bool isHealthy() = 0;
};

DEFINE_PTR_TYPES(IStorageService)

} // namespace k2eg::service::storage

#endif // K2EG_SERVICE_STORAGE_ISTORAGESERVICE_H_
