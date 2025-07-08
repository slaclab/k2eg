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

// Add stream operator for BackendType
inline std::ostream& operator<<(std::ostream& os, const BackendType& type) {
    switch (type) {
        case BackendType::MongoDB:
            return os << "MongoDB";
        default:
            return os << "Unknown";
    }
}

// Add conversion from string for boost::program_options
inline std::istream& operator>>(std::istream& is, BackendType& type) {
    std::string token;
    is >> token;
    if (token == "mongodb" || token == "MongoDB") {
        type = BackendType::MongoDB;
    } else {
        throw boost::program_options::validation_error(
            boost::program_options::validation_error::invalid_option_value,
            "backend-type", token);
    }
    return is;
}

struct StorageServiceConfiguration
{
    BackendType backend_type = BackendType::MongoDB; // Default to MongoDB
    ConstStorageImplementationConfigShrdPtr implementation_config; // Optional implementation-specific configuration
};

DEFINE_PTR_TYPES(StorageServiceConfiguration);

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

    /**
     * @brief Add program options for storage service configurations
     * @param desc Options description to add configurations to
     */
    static void addConfigurations(boost::program_options::options_description& desc);

    /**
     * @brief Get storage service configurations from program options
     * @param vm Variables map containing parsed options
     * @return Shared pointer to StorageServiceConfiguration
     */
    static StorageServiceConfigurationUPtr getConfigurations(const boost::program_options::variables_map& vm);

private:
};

} // namespace k2eg::service::storage

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
