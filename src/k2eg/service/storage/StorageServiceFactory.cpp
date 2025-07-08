#include <k2eg/common/utility.h>

#include <k2eg/service/ServiceResolver.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/storage/StorageServiceFactory.h>
#include <k2eg/service/storage/impl/MongoDBStorageService.h>

#include <stdexcept>

using namespace k2eg::service::storage;
using namespace k2eg::service::storage::impl;
using namespace k2eg::service::log;
using namespace k2eg::service;

// Static member definition

#define STORAGE_TYPE "storage-backend"
static BackendType backend_type;

IStorageServiceShrdPtr StorageServiceFactory::create(StorageServiceConfigurationUPtr config)
{
    auto logger = ServiceResolver<ILogger>::resolve();
    try
    {
        switch (backend_type)
        {
        case BackendType::MongoDB:
            logger->logMessage("Creating MongoDB storage service", LogLevel::INFO);
            return std::make_shared<MongoDBStorageService>(config->implementation_config);

        default: throw std::runtime_error("Unknown backend type");
        }
    }
    catch (const std::exception& e)
    {
        logger->logMessage(STRING_FORMAT("Failed to create storage service: {%1%}", e.what()), LogLevel::ERROR);
        throw;
    }
}

void StorageServiceFactory::addConfigurations(boost::program_options::options_description& desc)
{
    desc.add_options()(STORAGE_TYPE, boost::program_options::value<BackendType>(&backend_type)->default_value(BackendType::MongoDB), "Storage backend type (e.g., mongodb)");

    // add mongodb specific options
    impl::fill_mongodb_program_option(desc);
}

StorageServiceConfigurationUPtr StorageServiceFactory::getConfigurations(const boost::program_options::variables_map& vm)
{
    auto config = MakeStorageServiceConfigurationUPtr();

    if (vm.count(STORAGE_TYPE))
    {
        config->backend_type = vm[STORAGE_TYPE].as<BackendType>();
        switch (config->backend_type)
        {
        case BackendType::MongoDB: config->implementation_config = impl::get_mongodb_program_option(vm); break;
        default:
            throw std::runtime_error(STRING_FORMAT("Unsupported storage backend type: {%1%}", config->backend_type));
        }
    }

    return config;
}