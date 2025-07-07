#ifndef K2EG_SERVICE_STORAGE_STORAGESERVICEFACTORY_H_
#define K2EG_SERVICE_STORAGE_STORAGESERVICEFACTORY_H_

#include "k2eg/service/log/ILogger.h"
#include <k2eg/service/storage/IStorageService.h>
#include <k2eg/service/storage/impl/MongoDBStorageService.h>

#include <string>
#include <unordered_map>

namespace k2eg::service::storage {

/**
 * @brief Factory for creating storage service instances
 *
 * This factory allows creation of different storage backend implementations
 * based on configuration.
 */
class StorageServiceFactory
{
public:
    /**
     * @brief Storage backend types
     */
    enum class BackendType
    {
        MongoDB,
        SQLite,
        PostgreSQL,
        Memory // For testing
    };

    /**
     * @brief Create storage service by type name
     * @param backend_type String representation of backend type
     * @param config_json JSON configuration for the backend
     * @return Shared pointer to storage service
     */
    static IStorageServiceShrdPtr create(const std::string& backend_type, const std::string& config_json = "{}");

    /**
     * @brief Create storage service by enum type
     * @param backend_type Backend type enum
     * @param config_json JSON configuration for the backend
     * @return Shared pointer to storage service
     */
    static IStorageServiceShrdPtr create(BackendType backend_type, const std::string& config_json = "{}");

    /**
     * @brief Get list of available backend types
     */
    static std::vector<std::string> getAvailableBackends();

    /**
     * @brief Convert string to BackendType enum
     */
    static BackendType stringToBackendType(const std::string& backend_type);

    /**
     * @brief Convert BackendType enum to string
     */
    static std::string backendTypeToString(BackendType backend_type);

private:
    /**
     * @brief Create MongoDB storage service
     */
    static IStorageServiceShrdPtr createMongoDBService(const std::string& config_json);

    /**
     * @brief Create SQLite storage service (future implementation)
     */
    static IStorageServiceShrdPtr createSQLiteService(const std::string& config_json);

    /**
     * @brief Create in-memory storage service (for testing)
     */
    static IStorageServiceShrdPtr createMemoryService(const std::string& config_json);

    static const std::unordered_map<std::string, BackendType> backend_type_map_;
};

} // namespace k2eg::service::storage

#endif // K2EG_SERVICE_STORAGE_STORAGESERVICEFACTORY_H_
