#include <k2eg/common/utility.h>

#include <k2eg/service/storage/StorageServiceFactory.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/ServiceResolver.h>
#include <stdexcept>
#include <algorithm>

using namespace k2eg::service::storage;
using namespace k2eg::service::storage::impl;
using namespace k2eg::service::log;
using namespace k2eg::service;

// Static member definition
const std::unordered_map<std::string, StorageServiceFactory::BackendType> 
StorageServiceFactory::backend_type_map_ = {
    {"mongodb", BackendType::MongoDB},
    {"mongo", BackendType::MongoDB},
    {"sqlite", BackendType::SQLite},
    {"postgresql", BackendType::PostgreSQL},
    {"postgres", BackendType::PostgreSQL},
    {"memory", BackendType::Memory},
    {"inmemory", BackendType::Memory}
};
IStorageServiceShrdPtr StorageServiceFactory::create(const std::string& backend_type, 
                                                     const std::string& config_json)
{
    return create(stringToBackendType(backend_type), config_json);
}

IStorageServiceShrdPtr StorageServiceFactory::create(BackendType backend_type, 
                                                     const std::string& config_json)
{
    auto logger = ServiceResolver<ILogger>::resolve();
    try {
        switch (backend_type) {
            case BackendType::MongoDB:
                logger->logMessage("Creating MongoDB storage service", LogLevel::INFO);
                return createMongoDBService(config_json);
                
            case BackendType::SQLite:
                logger->logMessage("Creating SQLite storage service", LogLevel::INFO);
                return createSQLiteService(config_json);
                
            case BackendType::PostgreSQL:
                logger->logMessage("Creating PostgreSQL storage service", LogLevel::INFO);
                throw std::runtime_error("PostgreSQL backend not yet implemented");
                
            case BackendType::Memory:
                logger->logMessage("Creating in-memory storage service", LogLevel::INFO);
                return createMemoryService(config_json);
                
            default:
                throw std::runtime_error("Unknown backend type");
        }
    } catch (const std::exception& e) {
        logger->logMessage(STRING_FORMAT("Failed to create storage service: {%1%}", e.what()), LogLevel::ERROR);
        throw;
    }
}

std::vector<std::string> StorageServiceFactory::getAvailableBackends()
{
    std::vector<std::string> backends;
    backends.reserve(backend_type_map_.size());
    
    for (const auto& pair : backend_type_map_) {
        backends.push_back(pair.first);
    }
    
    std::sort(backends.begin(), backends.end());
    return backends;
}

StorageServiceFactory::BackendType StorageServiceFactory::stringToBackendType(const std::string& backend_type)
{
    std::string lower_type = backend_type;
    std::transform(lower_type.begin(), lower_type.end(), lower_type.begin(), ::tolower);
    
    auto it = backend_type_map_.find(lower_type);
    if (it == backend_type_map_.end()) {
        throw std::runtime_error("Unknown storage backend type: " + backend_type);
    }
    
    return it->second;
}

std::string StorageServiceFactory::backendTypeToString(BackendType backend_type)
{
    switch (backend_type) {
        case BackendType::MongoDB:
            return "mongodb";
        case BackendType::SQLite:
            return "sqlite";
        case BackendType::PostgreSQL:
            return "postgresql";
        case BackendType::Memory:
            return "memory";
        default:
            return "unknown";
    }
}

IStorageServiceShrdPtr StorageServiceFactory::createMongoDBService(const std::string& config_json)
{
    // Parse configuration from JSON
    // For now, use default configuration
    MongoDBStorageConfiguration config;
    
    // TODO: Parse config_json to override default values
    // Example JSON parsing would go here
    
    return std::make_shared<MongoDBStorageService>(config);
}

IStorageServiceShrdPtr StorageServiceFactory::createSQLiteService(const std::string& config_json)
{
    // TODO: Implement SQLite storage service
    throw std::runtime_error("SQLite storage backend not yet implemented");
}

IStorageServiceShrdPtr StorageServiceFactory::createMemoryService(const std::string& config_json)
{
    // TODO: Implement in-memory storage service for testing
    throw std::runtime_error("Memory storage backend not yet implemented");
}
