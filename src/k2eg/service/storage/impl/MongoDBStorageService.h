#ifndef K2EG_SERVICE_STORAGE_IMPL_MONGODBSTORAGESERVICE_H_
#define K2EG_SERVICE_STORAGE_IMPL_MONGODBSTORAGESERVICE_H_

#include "k2eg/service/log/ILogger.h"
#include <k2eg/service/storage/IStorageService.h>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/pool.hpp>
#include <memory>
#include <string>
#include <mutex>

namespace k2eg::service::storage::impl {

/**
 * @brief Configuration for MongoDB storage service
 */
struct MongoDBStorageConfiguration {
    std::string connection_string = "mongodb://localhost:27017";
    std::string database_name = "k2eg_archive";
    std::string collection_name = "epics_data";
    std::string index_collection_name = "epics_index";
    size_t connection_pool_size = 10;
    size_t batch_size = 1000;
    bool create_indexes = true;
};

/**
 * @brief MongoDB implementation of the storage service
 * 
 * This implementation uses MongoDB for storing archived EPICS data with
 * proper indexing for efficient queries.
 */
class MongoDBStorageService : public IStorageService {
private:
    log::ILoggerShrdPtr logger;    

    MongoDBStorageConfiguration config_;
    std::unique_ptr<mongocxx::instance> instance_;
    std::unique_ptr<mongocxx::pool> pool_;
    mutable std::mutex stats_mutex_;
    
    // Statistics
    size_t stored_records_count_ = 0;
    size_t failed_operations_count_ = 0;
    std::chrono::system_clock::time_point last_operation_time_;

    /**
     * @brief Create necessary indexes for optimal query performance
     */
    void createIndexes();

    /**
     * @brief Convert ArchiveRecord to BSON document
     */
    bsoncxx::document::value recordToBson(const ArchiveRecord& record);

    /**
     * @brief Convert BSON document to ArchiveRecord
     */
    ArchiveRecord bsonToRecord(const bsoncxx::document::view& doc);

public:
    explicit MongoDBStorageService(const MongoDBStorageConfiguration& config);
    ~MongoDBStorageService() override = default;

    // IStorageService interface
    bool initialize() override;
    void shutdown() override;
    bool store(const ArchiveRecord& record) override;
    size_t storeBatch(const std::vector<ArchiveRecord>& records) override;
    ArchiveQueryResult query(const ArchiveQuery& query) override;
    std::optional<ArchiveRecord> getLatest(const std::string& pv_name) override;
    std::string getStatistics() override;
    bool isHealthy() override;
};

DEFINE_PTR_TYPES(MongoDBStorageService)

} // namespace k2eg::service::storage::impl

#endif // K2EG_SERVICE_STORAGE_IMPL_MONGODBSTORAGESERVICE_H_
