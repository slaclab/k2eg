#include "k2eg/common/utility.h"
#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <chrono>
#include <k2eg/service/ServiceResolver.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/storage/impl/MongoDBStorageService.h>
#include <mongocxx/exception/exception.hpp>
#include <sstream>

using namespace k2eg::service::storage;
using namespace k2eg::service::storage::impl;
using namespace k2eg::service::log;
using namespace k2eg::service;

namespace bsoncxx_builder = bsoncxx::builder::stream;

MongoDBStorageService::MongoDBStorageService(const MongoDBStorageConfiguration& config)
    : logger(ServiceResolver<ILogger>::resolve()), config_(config), last_operation_time_(std::chrono::system_clock::now())
{
}

bool MongoDBStorageService::initialize()
{
    try
    {
        // Initialize MongoDB instance (should be done only once per application)
        instance_ = std::make_unique<mongocxx::instance>();

        // Create connection pool
        mongocxx::uri uri{config_.connection_string};
        pool_ = std::make_unique<mongocxx::pool>(uri);

        // Test connection
        auto client = pool_->acquire();
        auto db = (*client)[config_.database_name];

        // Ping the database to ensure connection
        auto ping_cmd = bsoncxx_builder::document{} << "ping" << 1 << bsoncxx_builder::finalize;
        db.run_command(ping_cmd.view());

        // Create indexes if configured
        if (config_.create_indexes)
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
    try
    {
        pool_.reset();
        instance_.reset();
        logger->logMessage("MongoDB storage service shut down successfully", LogLevel::INFO);
    }
    catch (const std::exception& e)
    {
        logger->logMessage(STRING_FORMAT("Error during MongoDB storage service shutdown: {%1%}", e.what()), LogLevel::ERROR);
    }
}

void MongoDBStorageService::createIndexes()
{
    try
    {
        auto client = pool_->acquire();
        auto collection = (*client)[config_.database_name][config_.collection_name];

        // Create compound index on pv_name and timestamp for efficient queries
        auto pv_time_index = bsoncxx_builder::document{} << "pv_name" << 1 << "timestamp" << 1 << bsoncxx_builder::finalize;
        collection.create_index(pv_time_index.view());

        // Create index on timestamp for time-range queries
        auto time_index = bsoncxx_builder::document{} << "timestamp" << 1 << bsoncxx_builder::finalize;
        collection.create_index(time_index.view());

        // Create index on topic for topic-based queries
        auto topic_index = bsoncxx_builder::document{} << "topic" << 1 << bsoncxx_builder::finalize;
        collection.create_index(topic_index.view());

        // Create unique index on message_id to prevent duplicates
        auto msg_id_index = bsoncxx_builder::document{} << "message_id" << 1 << bsoncxx_builder::finalize;
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

bsoncxx::document::value MongoDBStorageService::recordToBson(const ArchiveRecord& record)
{
    auto doc = bsoncxx_builder::document{};

    doc << "pv_name" << record.pv_name << "topic" << record.topic << "timestamp" << bsoncxx::types::b_date{record.timestamp} << "message_id"
        << static_cast<int64_t>(record.message_id) << "metadata" << record.metadata;

    // Store binary data as BSON binary
    if (!record.data.empty())
    {
        doc << "data"
            << bsoncxx::types::b_binary{
                   bsoncxx::binary_sub_type::k_binary, static_cast<uint32_t>(record.data.size()), record.data.data()};
    }

    return doc << bsoncxx_builder::finalize;
}

ArchiveRecord MongoDBStorageService::bsonToRecord(const bsoncxx::document::view& doc)
{
    ArchiveRecord record;

    if (auto pv_name = doc["pv_name"])
    {
        record.pv_name = pv_name.get_string().value;
    }

    if (auto topic = doc["topic"])
    {
        record.topic = topic.get_string().value;
    }

    if (auto timestamp = doc["timestamp"])
    {
        // Convert milliseconds since epoch to time_point
        record.timestamp = std::chrono::system_clock::time_point{std::chrono::milliseconds{timestamp.get_date().value.count()}};
    }

    if (auto message_id = doc["message_id"])
    {
        record.message_id = static_cast<uint64_t>(message_id.get_int64().value);
    }

    if (auto metadata = doc["metadata"])
    {
        record.metadata = metadata.get_string().value;
    }

    if (auto data = doc["data"])
    {
        auto binary = data.get_binary();
        record.data.assign(binary.bytes, binary.bytes + binary.size);
    }

    return record;
}

bool MongoDBStorageService::store(const ArchiveRecord& record)
{
    try
    {
        auto client = pool_->acquire();
        auto collection = (*client)[config_.database_name][config_.collection_name];

        auto doc = recordToBson(record);
        collection.insert_one(doc.view());

        std::lock_guard<std::mutex> lock(stats_mutex_);
        stored_records_count_++;
        last_operation_time_ = std::chrono::system_clock::now();

        return true;
    }
    catch (const mongocxx::exception& e)
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        failed_operations_count_++;
        logger->logMessage(STRING_FORMAT("Failed to store record for PV {%1%}: {%2%}", record.pv_name % e.what()), LogLevel::ERROR);
        return false;
    }
}

size_t MongoDBStorageService::storeBatch(const std::vector<ArchiveRecord>& records)
{

    if (records.empty())
    {
        return 0;
    }

    try
    {
        auto client = pool_->acquire();
        auto collection = (*client)[config_.database_name][config_.collection_name];

        std::vector<bsoncxx::document::value> documents;
        documents.reserve(records.size());

        for (const auto& record : records)
        {
            documents.push_back(recordToBson(record));
        }

        auto result = collection.insert_many(documents);

        std::lock_guard<std::mutex> lock(stats_mutex_);
        size_t                      inserted_count = result ? result->inserted_count() : 0;
        stored_records_count_ += inserted_count;
        failed_operations_count_ += (records.size() - inserted_count);
        last_operation_time_ = std::chrono::system_clock::now();

        return inserted_count;
    }
    catch (const mongocxx::exception& e)
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        failed_operations_count_ += records.size();
        logger->logMessage(STRING_FORMAT("Failed to store batch of {%1%} records: {%2%}", records.size() % e.what()), LogLevel::ERROR);
        return 0;
    }
}

ArchiveQueryResult MongoDBStorageService::query(const ArchiveQuery& query)
{
    ArchiveQueryResult result;

    try
    {
        auto client = pool_->acquire();
        auto collection = (*client)[config_.database_name][config_.collection_name];

        // Build query filter
        auto filter_builder = bsoncxx_builder::document{};

        if (!query.pv_name.empty())
        {
            filter_builder << "pv_name" << query.pv_name;
        }

        if (query.topic)
        {
            filter_builder << "topic" << *query.topic;
        }

        if (query.start_time || query.end_time)
        {
            auto time_filter = bsoncxx_builder::document{};
            if (query.start_time)
            {
                time_filter << "$gte" << bsoncxx::types::b_date{*query.start_time};
            }
            if (query.end_time)
            {
                time_filter << "$lte" << bsoncxx::types::b_date{*query.end_time};
            }
            filter_builder << "timestamp" << time_filter;
        }

        auto filter = filter_builder << bsoncxx_builder::finalize;

        // Set up query options
        mongocxx::options::find opts{};
        opts.sort(bsoncxx_builder::document{} << "timestamp" << 1 << bsoncxx_builder::finalize);

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

std::optional<ArchiveRecord> MongoDBStorageService::getLatest(const std::string& pv_name)
{
    try
    {
        auto client = pool_->acquire();
        auto collection = (*client)[config_.database_name][config_.collection_name];

        auto filter = bsoncxx_builder::document{} << "pv_name" << pv_name << bsoncxx_builder::finalize;

        mongocxx::options::find opts{};
        opts.sort(bsoncxx_builder::document{} << "timestamp" << -1 << bsoncxx_builder::finalize);
        opts.limit(1);

        auto cursor = collection.find(filter.view(), opts);
        auto it = cursor.begin();

        if (it != cursor.end())
        {
            return bsonToRecord(*it);
        }

        return std::nullopt;
    }
    catch (const mongocxx::exception& e)
    {
        logger->logMessage(STRING_FORMAT("Failed to get latest record for PV {%1%}: {%2%}", pv_name % e.what()), LogLevel::ERROR);
        return std::nullopt;
    }
}

std::string MongoDBStorageService::getStatistics()
{
    std::lock_guard<std::mutex> lock(stats_mutex_);

    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_operation_time_);

    std::stringstream ss;
    ss << "{" << "\"stored_records\": " << stored_records_count_ << "," << "\"failed_operations\": " << failed_operations_count_ << ","
       << "\"last_operation_seconds_ago\": " << duration.count() << "," << "\"database_name\": \"" << config_.database_name << "\"," << "\"collection_name\": \""
       << config_.collection_name << "\"" << "}";

    return ss.str();
}

bool MongoDBStorageService::isHealthy()
{
    try
    {
        auto client = pool_->acquire();
        auto db = (*client)[config_.database_name];

        // Simple ping to check connectivity
        auto ping_cmd = bsoncxx_builder::document{} << "ping" << 1 << bsoncxx_builder::finalize;
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
