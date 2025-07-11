#ifndef K2EG_SERVICE_STORAGE_STORAGESERVICEFACTORY_H_
#define K2EG_SERVICE_STORAGE_STORAGESERVICEFACTORY_H_

#include "k2eg/common/types.h"
#include <k2eg/service/storage/IStorageService.h>
#include <k2eg/service/storage/impl/MongoDBStorageService.h>

#include <boost/program_options.hpp>

#include <string>

namespace k2eg::service::storage {

/**
 * @brief Storage backend types
 */
enum class BackendType
{
    MongoDB
};

/**
 * @brief Output stream operator for BackendType
 *
 * This allows printing the BackendType to an output stream.
 * Useful for logging and debugging purposes.
 */
inline std::ostream& operator<<(std::ostream& os, const BackendType& type)
{
    switch (type)
    {
    case BackendType::MongoDB: return os << "MongoDB";
    default: return os << "Unknown";
    }
}

/**
 * @brief Input stream operator for BackendType
 *
 * This allows reading the BackendType from an input stream.
 * Useful for parsing configuration files or command line arguments.
 */
inline std::istream& operator>>(std::istream& is, BackendType& type)
{
    std::string token;
    is >> token;
    if (token == "mongodb" || token == "MongoDB")
    {
        type = BackendType::MongoDB;
    }
    else
    {
        throw boost::program_options::validation_error(boost::program_options::validation_error::invalid_option_value, "backend-type", token);
    }
    return is;
}

/**
 * @brief Configuration for storage service
 */
struct StorageServiceConfiguration
{
    /**
     * @brief Backend type for the storage service
     * This determines which storage implementation to use (e.g., MongoDB, SQLite)
     */
    BackendType backend_type = BackendType::MongoDB;
    /**
     * @brief Optional implementation-specific configuration
     * This can be used to pass additional parameters to the storage backend
     * (e.g., MongoDB connection string, SQLite file path, etc.)
     */
    ConstStorageImplementationConfigShrdPtr implementation_config;
};

DEFINE_PTR_TYPES(StorageServiceConfiguration);

/**
 * @brief Fill program options for storage service
 * @param desc Options description to fill
 */
void fill_storage_service_program_option(boost::program_options::options_description& desc);
/**
 * @brief Get storage service program options from variables map
 *
 * This function extracts storage service configuration from the provided variables map.
 * It throws an exception if the required section is missing.
 * 
 * @param vm Variables map containing parsed options
 * @return Unique pointer to StorageServiceConfiguration
 */
StorageServiceConfigurationUPtr get_storage_service_program_option(const boost::program_options::variables_map& vm);

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
     * @brief Create storage service by enum type
     * @param backend_type Backend type enum
     * @param config_json JSON configuration for the backend
     * @return Shared pointer to storage service
     */
    static IStorageServiceShrdPtr create(StorageServiceConfigurationUPtr config = nullptr);
private:
};

} // namespace k2eg::service::storage

// Extend boost::lexical_cast for BackendType
namespace boost {
template <>
inline std::string lexical_cast<std::string, k2eg::service::storage::BackendType>(const k2eg::service::storage::BackendType& type)
{
    switch (type)
    {
    case k2eg::service::storage::BackendType::MongoDB: return "mongodb";
    }
    throw boost::bad_lexical_cast(); // fallback
}

template <>
inline k2eg::service::storage::BackendType lexical_cast<k2eg::service::storage::BackendType, std::string>(const std::string& str)
{
    if (str == "mongodb" || str == "mongo")
        return k2eg::service::storage::BackendType::MongoDB;
    throw boost::bad_lexical_cast(); // fallback
}
} // namespace boost

#endif // K2EG_SERVICE_STORAGE_STORAGESERVICEFACTORY_H_
