#ifndef K2EG_SERVICE_STORAGE_IMPL_MONGODBSTORAGESERVICE_H_
#define K2EG_SERVICE_STORAGE_IMPL_MONGODBSTORAGESERVICE_H_

#include <k2eg/common/types.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/storage/IStorageService.h>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/pool.hpp>

#include <boost/program_options.hpp>

#include <memory>
#include <mutex>
#include <string>

namespace k2eg::service::storage::impl {

constexpr const char* MONGODB_CONNECTION_STRING_KEY = "connection-string";
constexpr const char* MONGODB_DATABASE_KEY = "database";
constexpr const char* MONGODB_COLLECTION_KEY = "collection";
constexpr const char* MONGODB_SNAPSHOTS_COLLECTION_KEY = "snapshots-collection";
constexpr const char* MONGODB_POOL_SIZE_KEY = "pool-size";
constexpr const char* MONGODB_TIMEOUT_MS_KEY = "timeout-ms";
constexpr const char* MONGODB_CREATE_INDEXES_KEY = "create-indexes";
constexpr const char* MONGODB_BATCH_SIZE_KEY = "batch-size";
constexpr const char* MONGODB_SECTION_KEY = "MongoDB";

/**
 * @brief Configuration for MongoDB storage service.
 */
struct MongoDBStorageImplementationConfig : public StorageImplementationConfig
{
    std::string connection_string = "mongodb://localhost:27017";
    std::string database_name = "k2eg_archive";
    std::string collection_name = "epics_data";
    std::string snapshots_collection_name = "snapshots";
    std::string index_collection_name = "epics_index";
    size_t      connection_pool_size = 10;
    size_t      batch_size = 1000;
    bool        create_indexes = true;
    int         timeout_ms = 5000;
};

DEFINE_PTR_TYPES(MongoDBStorageImplementationConfig);

/**
 * @brief Fill MongoDB program options.
 * @param desc Options description to populate.
 */
void fill_mongodb_program_option(boost::program_options::options_description& desc);
/**
 * @brief Extract MongoDB program options from variables map.
 * @param vm Variables map to read from.
 * @return Parsed storage implementation config (const shared pointer).
 */
ConstStorageImplementationConfigShrdPtr get_mongodb_program_option(const boost::program_options::variables_map& vm);

/**
 * @brief MongoDB implementation of the storage service.
 */
class MongoDBStorageService : public IStorageService
{
private:
    log::ILoggerShrdPtr logger; ///< Logger instance for logging

    ConstMongoDBStorageImplementationConfigShrdPtr config_;      ///< MongoDB storage configuration
    std::unique_ptr<mongocxx::instance>            instance_;    ///< MongoDB driver instance
    std::unique_ptr<mongocxx::pool>                pool_;        ///< MongoDB connection pool
    mutable std::mutex                             stats_mutex_; ///< Mutex for synchronizing access to statistics

    // Statistics
    size_t stored_records_count_ = 0;    ///< Count of successfully stored records
    size_t failed_operations_count_ = 0; ///< Count of failed operations

    /**
     * @brief Create necessary indexes for optimal query performance.
     */
    void createIndexes();

    /**
     * @brief Convert ArchiveRecord to BSON document.
     */
    bsoncxx::document::value recordToBson(const ArchiveRecord& record);

    /**
     * @brief Convert BSON document to ArchiveRecord.
     */
    ArchiveRecord bsonToRecord(const bsoncxx::document::view& doc);

    /**
     * @brief Convert Snapshot to BSON document.
     */
    bsoncxx::document::value snapshotToBson(const Snapshot& snapshot);

    /**
     * @brief Convert BSON document to Snapshot.
     */
    Snapshot bsonToSnapshot(const bsoncxx::document::view& doc);
    /**
     * @brief Initialize the service.
     */
    void initialize();
    /**
     * @brief Shut down the service.
     */
    void shutdown();

public:
    /**
     * @brief Construct a MongoDB storage service.
     * @param config Backend configuration (connection string, db/collections).
     */
    explicit MongoDBStorageService(ConstStorageImplementationConfigShrdPtr config);
    /**
     * @brief Destroy the service and release resources.
     */
    virtual ~MongoDBStorageService() override;
    // Overridden methods inherit documentation from IStorageService
    bool                    isHealthy() override;
    bool                    clearAllData() override;
    bool                    store(const ArchiveRecord& record) override;
    size_t                  storeBatch(const std::vector<ArchiveRecord>& records) override;
    ArchiveQueryResult      query(const ArchiveQuery& query) override;
    std::string             createSnapshot(const Snapshot& snapshot) override;
    bool                    deleteSnapshot(const std::string& snapshot_id) override;
    std::vector<Snapshot>   listSnapshots() override;
    std::optional<Snapshot> getSnapshot(const std::string& snapshot_id) override;
    std::optional<Snapshot> findSnapshotBySearchKey(const std::string& search_key) override;
    SnapshotIdRangeResult   listSnapshotIdsInRange(
          const std::chrono::system_clock::time_point& start_time,
          const std::chrono::system_clock::time_point& end_time,
          size_t                                       limit,
          const std::optional<std::string>&            continuation_token = std::nullopt) override;
};

DEFINE_PTR_TYPES(MongoDBStorageService)

} // namespace k2eg::service::storage::impl

#endif // K2EG_SERVICE_STORAGE_IMPL_MONGODBSTORAGESERVICE_H_
