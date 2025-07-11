#include "k2eg/common/utility.h"
#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <chrono>
#include <k2eg/service/ServiceResolver.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/storage/impl/MongoDBStorageService.h>
#include <mongocxx/exception/exception.hpp>


using namespace k2eg::service::storage;
using namespace k2eg::service::storage::impl;
using namespace k2eg::service::log;
using namespace k2eg::service;

namespace bsoncxx_builder = bsoncxx::builder::stream;

constexpr const char* MONGODB_CONNECTION_STRING_KEY = "connection-string";
constexpr const char* MONGODB_DATABASE_KEY = "database";
constexpr const char* MONGODB_COLLECTION_KEY = "collection";
constexpr const char* MONGODB_POOL_SIZE_KEY = "pool-size";
constexpr const char* MONGODB_TIMEOUT_MS_KEY = "timeout-ms";
constexpr const char* MONGODB_CREATE_INDEXES_KEY = "create-indexes";
constexpr const char* MONGODB_BATCH_SIZE_KEY = "batch-size";
constexpr const char* MONGODB_SECTION_KEY = "mongodb";

constexpr const char* DEFAULT_CONNECTION_STRING = "mongodb://localhost:27017";
constexpr const char* DEFAULT_DATABASE = "k2eg";
constexpr const char* DEFAULT_COLLECTION = "data";
constexpr int         DEFAULT_POOL_SIZE = 10;
constexpr int         DEFAULT_TIMEOUT_MS = 5000;
constexpr bool        DEFAULT_CREATE_INDEXES = true;
constexpr int         DEFAULT_BATCH_SIZE = 1000;

constexpr std::string BSON_PV_NAME_FIELD = "pv_name";
constexpr std::string BSON_TOPIC_FIELD = "topic";
constexpr std::string BSON_TIMESTAMP_FIELD = "timestamp";
constexpr std::string BSON_MESSAGE_ID_FIELD = "message_id";
constexpr std::string BSON_METADATA_FIELD = "metadata";
constexpr std::string BSON_DATA_FIELD = "data";
constexpr std::string BSON_PING_FIELD = "ping";

constexpr std::string MONGO_GTE_OPERATOR = "$gte";
constexpr std::string MONGO_LTE_OPERATOR = "$lte";

namespace k2eg::service::storage::impl {
    
#pragma region MongoDB Program Options
void fill_mongodb_program_option(boost::program_options::options_description& desc)
{
    // Create a dedicated section for MongoDB options
    boost::program_options::options_description mongodb_section("mongodb");

    mongodb_section.add_options()(MONGODB_CONNECTION_STRING_KEY, boost::program_options::value<std::string>()->default_value(DEFAULT_CONNECTION_STRING), "MongoDB connection string")(
        MONGODB_DATABASE_KEY, boost::program_options::value<std::string>()->default_value(DEFAULT_DATABASE), "MongoDB database name")(
        MONGODB_COLLECTION_KEY, boost::program_options::value<std::string>()->default_value(DEFAULT_COLLECTION), "MongoDB collection name")(
        MONGODB_POOL_SIZE_KEY, boost::program_options::value<int>()->default_value(DEFAULT_POOL_SIZE), "MongoDB connection pool size")(
        MONGODB_TIMEOUT_MS_KEY, boost::program_options::value<int>()->default_value(DEFAULT_TIMEOUT_MS), "MongoDB operation timeout in milliseconds")(
        MONGODB_CREATE_INDEXES_KEY, boost::program_options::value<bool>()->default_value(DEFAULT_CREATE_INDEXES), "Create database indexes automatically")(
        MONGODB_BATCH_SIZE_KEY, boost::program_options::value<int>()->default_value(DEFAULT_BATCH_SIZE), "Maximum batch size for bulk operations");

    // Add the MongoDB section to the main description
    desc.add(mongodb_section);
}

ConstStorageImplementationConfigShrdPtr get_mongodb_program_option(const boost::program_options::variables_map& vm)
{
    if (!vm.count(MONGODB_SECTION_KEY))
    {
        throw std::runtime_error("MongoDB configuration section is missing in the program options");
    }

    auto config = std::make_shared<MongoDBStorageImplementationConfig>();
    auto mongodb_vm = vm[MONGODB_SECTION_KEY].as<boost::program_options::variables_map>();

    // Extract MongoDB connection settings
    if (mongodb_vm.count(MONGODB_CONNECTION_STRING_KEY))
    {
        config->connection_string = mongodb_vm[MONGODB_CONNECTION_STRING_KEY].as<std::string>();
    }

    if (mongodb_vm.count(MONGODB_DATABASE_KEY))
    {
        config->database_name = mongodb_vm[MONGODB_DATABASE_KEY].as<std::string>();
    }

    if (mongodb_vm.count(MONGODB_COLLECTION_KEY))
    {
        config->collection_name = mongodb_vm[MONGODB_COLLECTION_KEY].as<std::string>();
    }

    if (mongodb_vm.count(MONGODB_POOL_SIZE_KEY))
    {
        config->connection_pool_size = static_cast<size_t>(mongodb_vm[MONGODB_POOL_SIZE_KEY].as<int>());
    }

    if (mongodb_vm.count(MONGODB_BATCH_SIZE_KEY))
    {
        config->batch_size = static_cast<size_t>(mongodb_vm[MONGODB_BATCH_SIZE_KEY].as<int>());
    }

    if (mongodb_vm.count(MONGODB_CREATE_INDEXES_KEY))
    {
        config->create_indexes = mongodb_vm[MONGODB_CREATE_INDEXES_KEY].as<bool>();
    }

    if (mongodb_vm.count(MONGODB_TIMEOUT_MS_KEY))
    {
        config->timeout_ms = mongodb_vm[MONGODB_TIMEOUT_MS_KEY].as<int>();
    }

    // Cast to base type for return
    return std::static_pointer_cast<const StorageImplementationConfig>(config);
}
} // namespace k2eg::service::storage::impl

#pragma region MongoDB Storage Serrvice Implementation
MongoDBStorageService::MongoDBStorageService(ConstStorageImplementationConfigShrdPtr config)
    : logger(ServiceResolver<ILogger>::resolve())
    , config_(std::static_pointer_cast<const MongoDBStorageImplementationConfig>(config))
{
    this->initialize();
}

MongoDBStorageService::~MongoDBStorageService()
{
    this->shutdown();
}

bool MongoDBStorageService::initialize()
{
    try
    {
        // Initialize MongoDB instance (should be done only once per application)
        instance_ = std::make_unique<mongocxx::instance>();

        // Create connection pool
        mongocxx::uri uri{config_->connection_string};
        pool_ = std::make_unique<mongocxx::pool>(uri);

        // Test connection
        auto client = pool_->acquire();
        auto db = (*client)[config_->database_name];

        // Ping the database to ensure connection
        auto ping_cmd = bsoncxx_builder::document{} << BSON_PING_FIELD << 1 << bsoncxx_builder::finalize;
        db.run_command(ping_cmd.view());

        // Create indexes if configured
        if (config_->create_indexes)
        {
            createIndexes();
        }

        logger->logMessage("MongoDB storage service initialized successfully", LogLevel::INFO);
        return true;
    }
    catch (const mongocxx::exception& e)
    {
        logger->logMessage(STRING_FORMAT("Failed to initialize MongoDB storage service: {%1%}", e.what()), LogLevel::ERROR);
        return false;
    }
    catch (const std::exception& e)
    {
        logger->logMessage(STRING_FORMAT("Failed to initialize MongoDB storage service: {%1%}", e.what()), LogLevel::ERROR);
        return false;
    }
}

void MongoDBStorageService::shutdown()
{
    // Clean shutdown - reset pool and instance
    if (pool_) {
        pool_.reset();
    }
    if (instance_) {
        instance_.reset();
    }
    logger->logMessage("MongoDB storage service shut down", LogLevel::INFO);
}

void MongoDBStorageService::createIndexes()
{
    try
    {
        auto client = pool_->acquire();
        auto collection = (*client)[config_->database_name][config_->collection_name];

        // Create compound index on pv_name and timestamp for efficient queries
        auto pv_time_index = bsoncxx_builder::document{} << BSON_PV_NAME_FIELD << 1 << BSON_TIMESTAMP_FIELD << 1 << bsoncxx_builder::finalize;
        collection.create_index(pv_time_index.view());

        // Create index on timestamp for time-range queries
        auto time_index = bsoncxx_builder::document{} << BSON_TIMESTAMP_FIELD << 1 << bsoncxx_builder::finalize;
        collection.create_index(time_index.view());

        // Create index on topic for topic-based queries
        auto topic_index = bsoncxx_builder::document{} << BSON_TOPIC_FIELD << 1 << bsoncxx_builder::finalize;
        collection.create_index(topic_index.view());

        // Create unique index on message_id to prevent duplicates
        auto msg_id_index = bsoncxx_builder::document{} << BSON_MESSAGE_ID_FIELD << 1 << bsoncxx_builder::finalize;
        auto unique_options = mongocxx::options::index{};
        unique_options.unique(true);
        collection.create_index(msg_id_index.view(), unique_options);

        logger->logMessage("MongoDB indexes created successfully", LogLevel::INFO);
    }
    catch (const mongocxx::exception& e)
    {
        logger->logMessage(STRING_FORMAT("Failed to create MongoDB indexes: {%1%}", e.what()), LogLevel::ERROR);
    }
}


bool MongoDBStorageService::store(const ArchiveRecord& record)
{
    try
    {
        auto client = pool_->acquire();
        auto collection = (*client)[config_->database_name][config_->collection_name];
        
        auto doc = recordToBson(record);
        collection.insert_one(doc.view());
        
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stored_records_count_++;
        }
        
        return true;
    }
    catch (const mongocxx::exception& e)
    {
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            failed_operations_count_++;
        }
        logger->logMessage(STRING_FORMAT("Failed to store record: {%1%}", e.what()), LogLevel::ERROR);
        return false;
    }
}

size_t MongoDBStorageService::storeBatch(const std::vector<ArchiveRecord>& records)
{
    if (records.empty()) return 0;
    
    try
    {
        auto client = pool_->acquire();
        auto collection = (*client)[config_->database_name][config_->collection_name];
        
        std::vector<bsoncxx::document::value> docs;
        docs.reserve(records.size());
        
        for (const auto& record : records)
        {
            docs.push_back(recordToBson(record));
        }
        
        auto result = collection.insert_many(docs);
        size_t inserted_count = result ? result->inserted_count() : 0;
        
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stored_records_count_ += inserted_count;
        }
        
        return inserted_count;
    }
    catch (const mongocxx::exception& e)
    {
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            failed_operations_count_++;
        }
        logger->logMessage(STRING_FORMAT("Failed to store batch: {%1%}", e.what()), LogLevel::ERROR);
        return 0;
    }
}

bsoncxx::document::value MongoDBStorageService::recordToBson(const ArchiveRecord& record)
{
    auto doc = bsoncxx_builder::document{};

    doc << BSON_PV_NAME_FIELD << record.pv_name << BSON_TOPIC_FIELD << record.topic << BSON_TIMESTAMP_FIELD
        << bsoncxx::types::b_date{record.timestamp} << BSON_MESSAGE_ID_FIELD << static_cast<int64_t>(record.message_id) << BSON_METADATA_FIELD
        << record.metadata;

    // Store binary data as BSON binary
    if (!record.data.empty())
    {
        doc << BSON_DATA_FIELD
            << bsoncxx::types::b_binary{
                   bsoncxx::binary_sub_type::k_binary, static_cast<uint32_t>(record.data.size()), record.data.data()};
    }

    return doc << bsoncxx_builder::finalize;
}

ArchiveRecord MongoDBStorageService::bsonToRecord(const bsoncxx::document::view& doc)
{
    ArchiveRecord record;

    if (auto pv_name = doc[BSON_PV_NAME_FIELD])
    {
        record.pv_name = pv_name.get_string().value;
    }

    if (auto topic = doc[BSON_TOPIC_FIELD])
    {
        record.topic = topic.get_string().value;
    }

    if (auto timestamp = doc[BSON_TIMESTAMP_FIELD])
    {
        // Convert milliseconds since epoch to time_point
        record.timestamp = std::chrono::system_clock::time_point{std::chrono::milliseconds{timestamp.get_date().value.count()}};
    }

    if (auto message_id = doc[BSON_MESSAGE_ID_FIELD])
    {
        record.message_id = static_cast<uint64_t>(message_id.get_int64().value);
    }

    if (auto metadata = doc[BSON_METADATA_FIELD])
    {
        record.metadata = metadata.get_string().value;
    }

    if (auto data = doc[BSON_DATA_FIELD])
    {
        auto binary = data.get_binary();
        record.data.assign(binary.bytes, binary.bytes + binary.size);
    }

    return record;
}

// Update query method to use constants
ArchiveQueryResult MongoDBStorageService::query(const ArchiveQuery& query)
{
    ArchiveQueryResult result;

    try
    {
        auto client = pool_->acquire();
        auto collection = (*client)[config_->database_name][config_->collection_name];

        // Build query filter
        auto filter_builder = bsoncxx_builder::document{};

        if (!query.pv_name.empty())
        {
            filter_builder << BSON_PV_NAME_FIELD << query.pv_name;
        }

        if (query.topic)
        {
            filter_builder << BSON_TOPIC_FIELD << *query.topic;
        }

        if (query.start_time || query.end_time)
        {
            auto time_filter = bsoncxx_builder::document{};
            if (query.start_time)
            {
                time_filter << MONGO_GTE_OPERATOR << bsoncxx::types::b_date{*query.start_time};
            }
            if (query.end_time)
            {
                time_filter << MONGO_LTE_OPERATOR << bsoncxx::types::b_date{*query.end_time};
            }
            filter_builder << BSON_TIMESTAMP_FIELD << time_filter;
        }

        auto filter = filter_builder << bsoncxx_builder::finalize;

        // Set up query options
        mongocxx::options::find opts{};
        opts.sort(bsoncxx_builder::document{} << BSON_TIMESTAMP_FIELD << 1 << bsoncxx_builder::finalize);

        if (query.limit)
        {
            opts.limit(static_cast<int64_t>(*query.limit));
        }

        // Execute query
        auto cursor = collection.find(filter.view(), opts);

        for (const auto& doc : cursor)
        {
            result.records.push_back(bsonToRecord(doc));
        }

        // Get total count (this is expensive for large collections)
        result.total_count = collection.count_documents(filter.view());
        result.has_more = query.limit && result.records.size() == *query.limit;

        return result;
    }
    catch (const mongocxx::exception& e)
    {
        logger->logMessage(STRING_FORMAT("Failed to query records: {%1%}", e.what()), LogLevel::ERROR);
        return result; // Return empty result
    }
}

// Update health check to use constants
bool MongoDBStorageService::isHealthy()
{
    try
    {
        auto client = pool_->acquire();
        auto db = (*client)[config_->database_name];

        // Simple ping to check connectivity
        auto ping_cmd = bsoncxx_builder::document{} << BSON_PING_FIELD << 1 << bsoncxx_builder::finalize;
        db.run_command(ping_cmd.view());

        return true;
    }
    catch (const mongocxx::exception& e)
    {
        return false;
    }
    catch (const std::exception& e)
    {
        return false;
    }
}
