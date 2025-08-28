#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <chrono>
#include <k2eg/common/uuid.h>
#include <k2eg/common/utility.h>
#include <mutex>

#include <k2eg/service/ServiceResolver.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/storage/impl/MongoDBStorageService.h>
#include <mongocxx/exception/exception.hpp>
#include <mongocxx/instance.hpp>

using namespace k2eg::service::storage;
using namespace k2eg::service::storage::impl;
using namespace k2eg::service::log;
using namespace k2eg::service;

namespace bsoncxx_builder = bsoncxx::builder::stream;

constexpr const char* DEFAULT_CONNECTION_STRING = "mongodb://localhost:27017";
constexpr const char* DEFAULT_DATABASE = "k2eg";
constexpr const char* DEFAULT_COLLECTION = "data";
constexpr const char* DEFAULT_SNAPSHOTS_COLLECTION = "snapshots";
constexpr int         DEFAULT_POOL_SIZE = 10;
constexpr int         DEFAULT_TIMEOUT_MS = 5000;
constexpr bool        DEFAULT_CREATE_INDEXES = true;
constexpr int         DEFAULT_BATCH_SIZE = 1000;

constexpr std::string BSON_PV_NAME_FIELD = "pv_name";
constexpr std::string BSON_TOPIC_FIELD = "topic";
constexpr std::string BSON_TIMESTAMP_FIELD = "timestamp";
constexpr std::string BSON_METADATA_FIELD = "metadata";
constexpr std::string BSON_DATA_FIELD = "data";
constexpr std::string BSON_SERIALIZATION_TYPE_FIELD = "ser_type";
constexpr std::string BSON_SNAPSHOT_ID_FIELD = "snapshot_id";
constexpr std::string BSON_PING_FIELD = "ping";

// Snapshot-specific fields
constexpr std::string BSON_SNAPSHOT_NAME_FIELD = "snapshot_name";
constexpr std::string BSON_CREATED_AT_FIELD = "created_at";
constexpr std::string BSON_DESCRIPTION_FIELD = "description";
constexpr std::string BSON_PV_NAMES_FIELD = "pv_names";
constexpr std::string BSON_SEARCH_KEY_FIELD = "search_key";

constexpr std::string MONGO_GTE_OPERATOR = "$gte";
constexpr std::string MONGO_LTE_OPERATOR = "$lte";

// Manage mongocxx::instance as a process-wide singleton to avoid
// "cannot create a mongocxx::instance object if one has already been created"
// exceptions when multiple storage services/tests are active in the same process.

namespace {
// Returns a reference to the process-wide mongocxx::instance.
// Constructed once and shared for the program lifetime.
inline mongocxx::instance& mongo_global_instance()
{
    static std::once_flag init_once;
    static std::unique_ptr<mongocxx::instance> instance_ptr;
    std::call_once(
        init_once,
        []()
        {
            instance_ptr = std::make_unique<mongocxx::instance>();
        });
    return *instance_ptr;
}

inline void create_index_safely(mongocxx::collection& coll, const bsoncxx::document::view_or_value& keys)
{
    try
    {
        coll.create_index(keys);
    }
    catch (const mongocxx::exception&)
    {
    }
}

// Time utilities: normalize to UTC
inline std::chrono::system_clock::time_point to_utc(std::chrono::system_clock::time_point tp_local)
{
    // Mongo stores dates in UTC; interpret input as local and convert to UTC.
    // Approach: convert local time_t to tm using localtime, then treat as UTC with timegm-like logic.
    // Portable workaround: compute offset between local and UTC at that time and subtract it.
    using namespace std::chrono;
    auto    tt = system_clock::to_time_t(tp_local);
    std::tm local_tm{};
    std::tm utc_tm{};
#if defined(_WIN32)
    localtime_s(&local_tm, &tt);
    gmtime_s(&utc_tm, &tt);
#else
    localtime_r(&tt, &local_tm);
    gmtime_r(&tt, &utc_tm);
#endif
    // Convert both back to time_t assuming they are local/utc respectively
    auto make_time = [](std::tm tmv)
    {
#if defined(__APPLE__) || defined(__linux__)
        return timegm(&tmv); // available on POSIX/Apple
#else
        // Fallback: mktime assumes tm is local; adjust after
        return mktime(&tmv);
#endif
    };
#if defined(__APPLE__) || defined(__linux__)
    time_t utc_tt = make_time(utc_tm);
#else
    time_t utc_tt = _mkgmtime(&utc_tm);
#endif
    time_t local_tt = mktime(&local_tm);
    // Offset local - utc in seconds
    auto offset = static_cast<long>(difftime(local_tt, utc_tt));
    return tp_local - seconds(offset);
}

inline void create_index_safely(mongocxx::collection& coll, const bsoncxx::document::view_or_value& keys, mongocxx::options::index&& opts)
{
    try
    {
        coll.create_index(keys, std::move(opts));
    }
    catch (const mongocxx::exception&)
    {
    }
}
} // namespace

namespace k2eg::service::storage::impl {

#pragma region MongoDB Program Options

void fill_mongodb_program_option(boost::program_options::options_description& desc)
{
    // Create a dedicated section for MongoDB options
    boost::program_options::options_description mongodb_section(MONGODB_SECTION_KEY);

    mongodb_section.add_options()(MONGODB_CONNECTION_STRING_KEY, boost::program_options::value<std::string>()->default_value(DEFAULT_CONNECTION_STRING), "MongoDB connection string")(
        MONGODB_DATABASE_KEY, boost::program_options::value<std::string>()->default_value(DEFAULT_DATABASE), "MongoDB database name")(
        MONGODB_COLLECTION_KEY, boost::program_options::value<std::string>()->default_value(DEFAULT_COLLECTION), "MongoDB collection name")(
        MONGODB_SNAPSHOTS_COLLECTION_KEY, boost::program_options::value<std::string>()->default_value(DEFAULT_SNAPSHOTS_COLLECTION), "MongoDB snapshots collection name")(
        MONGODB_POOL_SIZE_KEY, boost::program_options::value<int>()->default_value(DEFAULT_POOL_SIZE), "MongoDB connection pool size")(
        MONGODB_TIMEOUT_MS_KEY, boost::program_options::value<int>()->default_value(DEFAULT_TIMEOUT_MS), "MongoDB operation timeout in milliseconds")(
        MONGODB_CREATE_INDEXES_KEY, boost::program_options::value<bool>()->default_value(DEFAULT_CREATE_INDEXES), "Create database indexes automatically")(
        MONGODB_BATCH_SIZE_KEY, boost::program_options::value<int>()->default_value(DEFAULT_BATCH_SIZE), "Maximum batch size for bulk operations");

    // Add the MongoDB section to the main description
    desc.add(mongodb_section);
}

ConstStorageImplementationConfigShrdPtr
get_mongodb_program_option(const boost::program_options::variables_map& vm)
{
    auto config = std::make_shared<MongoDBStorageImplementationConfig>();
    // Extract MongoDB connection settings
    if (vm.count(MONGODB_CONNECTION_STRING_KEY))
    {
        config->connection_string = vm[MONGODB_CONNECTION_STRING_KEY].as<std::string>();
    }

    if (vm.count(MONGODB_DATABASE_KEY))
    {
        config->database_name = vm[MONGODB_DATABASE_KEY].as<std::string>();
    }

    if (vm.count(MONGODB_COLLECTION_KEY))
    {
        config->collection_name = vm[MONGODB_COLLECTION_KEY].as<std::string>();
    }

    if (vm.count(MONGODB_SNAPSHOTS_COLLECTION_KEY))
    {
        config->snapshots_collection_name = vm[MONGODB_SNAPSHOTS_COLLECTION_KEY].as<std::string>();
    }

    if (vm.count(MONGODB_POOL_SIZE_KEY))
    {
        config->connection_pool_size = static_cast<size_t>(vm[MONGODB_POOL_SIZE_KEY].as<int>());
    }

    if (vm.count(MONGODB_BATCH_SIZE_KEY))
    {
        config->batch_size = static_cast<size_t>(vm[MONGODB_BATCH_SIZE_KEY].as<int>());
    }

    if (vm.count(MONGODB_CREATE_INDEXES_KEY))
    {
        config->create_indexes = vm[MONGODB_CREATE_INDEXES_KEY].as<bool>();
    }

    if (vm.count(MONGODB_TIMEOUT_MS_KEY))
    {
        config->timeout_ms = vm[MONGODB_TIMEOUT_MS_KEY].as<int>();
    }

    // Cast to base type for return
    return std::static_pointer_cast<const StorageImplementationConfig>(config);
}
} // namespace k2eg::service::storage::impl

#pragma region MongoDB Storage Serrvice Implementation

MongoDBStorageService::MongoDBStorageService(ConstStorageImplementationConfigShrdPtr config)
    : logger(ServiceResolver<ILogger>::resolve()), config_(std::static_pointer_cast<const MongoDBStorageImplementationConfig>(config))
{
    this->initialize();
}

MongoDBStorageService::~MongoDBStorageService()
{
    this->shutdown();
}

void MongoDBStorageService::initialize()
{
    try
    {
        // Ensure process-wide MongoDB driver instance exists
        (void)mongo_global_instance();

        // Create connection pool
        mongocxx::uri uri{config_->connection_string};
        pool_ = std::make_unique<mongocxx::pool>(uri);

        // Test connection
        auto client = pool_->acquire();
        auto db = (*client)[config_->database_name];

        // Ping the database to ensure connection
        auto ping_cmd = bsoncxx_builder::document{} << BSON_PING_FIELD << 1 << bsoncxx_builder::finalize;
        db.run_command(ping_cmd.view());

        logger->logMessage("MongoDB connection established successfully", LogLevel::INFO);

        // Create indexes if configured
        if (config_->create_indexes)
        {
            createIndexes();
        }

        logger->logMessage("MongoDB storage service initialized successfully", LogLevel::INFO);
    }
    catch (const mongocxx::exception& e)
    {
        logger->logMessage(STRING_FORMAT("Failed to initialize MongoDB storage service: {%1%}", e.what()), LogLevel::ERROR);
        throw std::runtime_error(STRING_FORMAT("MongoDB initialization failed: {%1%}", e.what()));
    }
    catch (const std::exception& e)
    {
        logger->logMessage(STRING_FORMAT("Failed to initialize MongoDB storage service: {%1%}", e.what()), LogLevel::ERROR);
        throw std::runtime_error(STRING_FORMAT("MongoDB initialization failed: {%1%}", e.what()));
    }
}

void MongoDBStorageService::shutdown()
{
    // Clean shutdown - reset pool and instance
    if (pool_)
    {
        pool_.reset();
    }
    logger->logMessage("MongoDB storage service shut down", LogLevel::INFO);
}

void MongoDBStorageService::createIndexes()
{
    static std::once_flag indexes_once;
    try
    {
        std::call_once(
            indexes_once,
            [this]()
            {
                auto client = pool_->acquire();
                auto collection = (*client)[config_->database_name][config_->collection_name];
                auto snapshots_collection = (*client)[config_->database_name][config_->snapshots_collection_name];

                // Create compound index on pv_name and timestamp for efficient queries
                auto pv_time_index = bsoncxx_builder::document{} << BSON_PV_NAME_FIELD << 1 << BSON_TIMESTAMP_FIELD << 1 << bsoncxx_builder::finalize;
                create_index_safely(collection, pv_time_index.view());

                // Create index on timestamp for time-range queries
                auto time_index = bsoncxx_builder::document{} << BSON_TIMESTAMP_FIELD << 1 << bsoncxx_builder::finalize;
                create_index_safely(collection, time_index.view());

                // Create index on topic for topic-based queries
                auto topic_index = bsoncxx_builder::document{} << BSON_TOPIC_FIELD << 1 << bsoncxx_builder::finalize;
                create_index_safely(collection, topic_index.view());

                // Create index on snapshot_id for snapshot-based queries
                auto snapshot_index = bsoncxx_builder::document{} << BSON_SNAPSHOT_ID_FIELD << 1 << bsoncxx_builder::finalize;
                create_index_safely(collection, snapshot_index.view());

                // Create compound index on snapshot_id and pv_name for efficient snapshot queries
                auto snapshot_pv_index = bsoncxx_builder::document{} << BSON_SNAPSHOT_ID_FIELD << 1 << BSON_PV_NAME_FIELD << 1 << bsoncxx_builder::finalize;
                create_index_safely(collection, snapshot_pv_index.view());

                // Create composite unique index to prevent duplicates (matches our upsert filter)
                auto unique_record_index = bsoncxx_builder::document{}
                                           << BSON_PV_NAME_FIELD << 1
                                           << BSON_TIMESTAMP_FIELD << 1
                                           << BSON_TOPIC_FIELD << 1
                                           << BSON_SNAPSHOT_ID_FIELD << 1
                                           << bsoncxx_builder::finalize;
                mongocxx::options::index unique_index_options{};
                unique_index_options.unique(true);
                unique_index_options.sparse(true); // Allow records without snapshot_id
                create_index_safely(collection, unique_record_index.view(), std::move(unique_index_options));

                // Create indexes for snapshots collection
                // Create unique index on snapshot_id
                auto snapshot_id_index = bsoncxx_builder::document{} << "_id" << 1 << bsoncxx_builder::finalize;

                // Create index on snapshot name for efficient lookups
                auto snapshot_name_index = bsoncxx_builder::document{} << BSON_SNAPSHOT_NAME_FIELD << 1 << bsoncxx_builder::finalize;
                create_index_safely(snapshots_collection, snapshot_name_index.view());

                // Create unique index on search_key for efficient snapshot lookups by search key
                auto                     search_key_index = bsoncxx_builder::document{} << BSON_SEARCH_KEY_FIELD << 1 << bsoncxx_builder::finalize;
                mongocxx::options::index search_key_index_options{};
                search_key_index_options.unique(true); // Make it unique since search_key should be unique per snapshot
                create_index_safely(snapshots_collection, search_key_index.view(), std::move(search_key_index_options));

                // Create index on created_at for time-based sorting
                auto snapshot_time_index = bsoncxx_builder::document{} << BSON_CREATED_AT_FIELD << 1 << bsoncxx_builder::finalize;
                create_index_safely(snapshots_collection, snapshot_time_index.view());

                logger->logMessage("MongoDB indexes created (idempotent)", LogLevel::INFO);
            });
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

        // Create a unique filter based on pv_name, timestamp, topic, and snapshot_id
        auto filter = bsoncxx_builder::document{};
        filter << BSON_PV_NAME_FIELD << record.pv_name
               << BSON_TIMESTAMP_FIELD << bsoncxx::types::b_date{record.timestamp}
               << BSON_TOPIC_FIELD << record.topic;

        if (record.snapshot_id)
        {
            filter << BSON_SNAPSHOT_ID_FIELD << *record.snapshot_id;
        }

        // Use replace_one with upsert to handle duplicates gracefully
        mongocxx::options::replace replace_options{};
        replace_options.upsert(true);

        auto result = collection.replace_one(filter.view(), doc.view(), replace_options);

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

size_t
MongoDBStorageService::storeBatch(const std::vector<ArchiveRecord>& records)
{
    if (records.empty())
        return 0;

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

        auto   result = collection.insert_many(docs);
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

bsoncxx::document::value
MongoDBStorageService::recordToBson(const ArchiveRecord& record)
{
    auto doc = bsoncxx_builder::document{};

    // Normalize timestamp to UTC before persisting
    doc << BSON_PV_NAME_FIELD << record.pv_name << BSON_TOPIC_FIELD << record.topic << BSON_TIMESTAMP_FIELD
        << bsoncxx::types::b_date{to_utc(record.timestamp)} << BSON_METADATA_FIELD << record.metadata;

    // Add snapshot_id if present
    if (record.snapshot_id)
    {
        doc << BSON_SNAPSHOT_ID_FIELD << *record.snapshot_id;
    }

    // Store binary data as BSON binary
    if (record.data)
    {
        auto data_ptr = record.data->data();
        if (data_ptr && data_ptr->size() > 0)
        {
            doc << BSON_DATA_FIELD
                << bsoncxx::types::b_binary{bsoncxx::binary_sub_type::k_binary, static_cast<uint32_t>(data_ptr->size()), reinterpret_cast<const uint8_t*>(data_ptr->data())};
            // Persist serialization type alongside data
            doc << BSON_SERIALIZATION_TYPE_FIELD << k2eg::common::serialization_to_string(record.data->serializationType());
        }
    }

    return doc << bsoncxx_builder::finalize;
}

ArchiveRecord
MongoDBStorageService::bsonToRecord(const bsoncxx::document::view& doc)
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

    if (auto metadata = doc[BSON_METADATA_FIELD])
    {
        record.metadata = metadata.get_string().value;
    }

    // Extract snapshot_id if present
    if (auto snapshot_id = doc[BSON_SNAPSHOT_ID_FIELD])
    {
        record.snapshot_id = std::string{snapshot_id.get_string().value};
    }

    if (auto data = doc[BSON_DATA_FIELD])
    {
        auto binary = data.get_binary();
        // Try to read serialization type; default to Unknown if missing
        k2eg::common::SerializationType ser = k2eg::common::SerializationType::Unknown;
        if (auto serf = doc[BSON_SERIALIZATION_TYPE_FIELD])
        {
            try
            {
                ser = k2eg::common::serialization_from_string(std::string{serf.get_string().value});
            }
            catch (...)
            {
                ser = k2eg::common::SerializationType::Unknown;
            }
        }
        record.data = std::make_unique<StoredMessage>(reinterpret_cast<const char*>(binary.bytes), binary.size, ser);
    }

    return record;
}

// Update query method to use constants
ArchiveQueryResult
MongoDBStorageService::query(const ArchiveQuery& query)
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

        if (query.snapshot_id)
        {
            filter_builder << BSON_SNAPSHOT_ID_FIELD << *query.snapshot_id;
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

bool MongoDBStorageService::clearAllData()
{
    try
    {
        auto client = pool_->acquire();
        auto db = (*client)[config_->database_name];

        // Delete all documents from data and snapshots collections
        auto data_collection = db[config_->collection_name];
        auto snap_collection = db[config_->snapshots_collection_name];

        auto res1 = data_collection.delete_many({});
        auto res2 = snap_collection.delete_many({});

        size_t del1 = res1 ? res1->deleted_count() : 0;
        size_t del2 = res2 ? res2->deleted_count() : 0;

        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stored_records_count_ = 0;
            // We don't reset failed_operations_count_ as it is diagnostic
        }

        logger->logMessage(STRING_FORMAT("Cleared MongoDB collections. data={%1%}, snapshots={%2%}", del1 % del2), LogLevel::INFO);
        return true;
    }
    catch (const mongocxx::exception& e)
    {
        logger->logMessage(STRING_FORMAT("Failed to clear MongoDB data: {%1%}", e.what()), LogLevel::ERROR);
        return false;
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

// Snapshot management methods implementation

std::string
MongoDBStorageService::createSnapshot(const Snapshot& snapshot)
{
    try
    {
        auto client = pool_->acquire();
        auto snapshots_collection = (*client)[config_->database_name][config_->snapshots_collection_name];

        // Let MongoDB assign _id if not provided
        auto doc = snapshotToBson(snapshot);
        auto result = snapshots_collection.insert_one(doc.view());

        if (result)
        {
            std::string sid;
            auto        idv = result->inserted_id();
            try
            {
                if (idv.type() == bsoncxx::type::k_oid)
                {
                    sid = idv.get_oid().value.to_string();
                }
                else if (idv.type() == bsoncxx::type::k_string)
                {
                    sid = std::string{idv.get_string().value};
                }
            }
            catch (...)
            {
                sid.clear();
            }
            logger->logMessage(STRING_FORMAT("Created snapshot with ID: {%1%}", sid), LogLevel::INFO);
            return sid;
        }
        else
        {
            logger->logMessage("Failed to create snapshot - no result returned", LogLevel::ERROR);
            return "";
        }
    }
    catch (const mongocxx::exception& e)
    {
        logger->logMessage(STRING_FORMAT("Failed to create snapshot: {%1%}", e.what()), LogLevel::ERROR);
        return "";
    }
}

bool MongoDBStorageService::deleteSnapshot(const std::string& snapshot_id)
{
    try
    {
        auto client = pool_->acquire();
        auto snapshots_collection = (*client)[config_->database_name][config_->snapshots_collection_name];

        // Support both ObjectId and legacy string _id
        bsoncxx_builder::document filter;
        try
        {
            filter << "_id" << bsoncxx::oid{snapshot_id} << bsoncxx_builder::finalize;
        }
        catch (...)
        {
            filter.clear();
            filter << "_id" << snapshot_id << bsoncxx_builder::finalize;
        }
        auto result = snapshots_collection.delete_one(filter.view());

        if (result && result->deleted_count() > 0)
        {
            logger->logMessage(STRING_FORMAT("Deleted snapshot with ID: {%1%}", snapshot_id), LogLevel::INFO);
            return true;
        }
        else
        {
            logger->logMessage(STRING_FORMAT("Snapshot not found for deletion: {%1%}", snapshot_id), LogLevel::ERROR);
            return false;
        }
    }
    catch (const mongocxx::exception& e)
    {
        logger->logMessage(STRING_FORMAT("Failed to delete snapshot: {%1%}", e.what()), LogLevel::ERROR);
        return false;
    }
}

std::vector<Snapshot>
MongoDBStorageService::listSnapshots()
{
    std::vector<Snapshot> snapshots;

    try
    {
        auto client = pool_->acquire();
        auto snapshots_collection = (*client)[config_->database_name][config_->snapshots_collection_name];

        mongocxx::options::find opts{};
        opts.sort(bsoncxx_builder::document{} << BSON_CREATED_AT_FIELD << -1 << bsoncxx_builder::finalize); // Sort by creation time descending

        auto cursor = snapshots_collection.find({}, opts);
        for (const auto& doc : cursor)
        {
            snapshots.push_back(bsonToSnapshot(doc));
        }

        logger->logMessage(STRING_FORMAT("Retrieved {%1%} snapshots", snapshots.size()), LogLevel::DEBUG);
    }
    catch (const mongocxx::exception& e)
    {
        logger->logMessage(STRING_FORMAT("Failed to list snapshots: {%1%}", e.what()), LogLevel::ERROR);
    }

    return snapshots;
}

std::optional<Snapshot>
MongoDBStorageService::getSnapshot(const std::string& snapshot_id)
{
    try
    {
        auto client = pool_->acquire();
        auto snapshots_collection = (*client)[config_->database_name][config_->snapshots_collection_name];

        bsoncxx_builder::document filter;
        try
        {
            filter << "_id" << bsoncxx::oid{snapshot_id} << bsoncxx_builder::finalize;
        }
        catch (...)
        {
            filter.clear();
            filter << "_id" << snapshot_id << bsoncxx_builder::finalize;
        }
        auto result = snapshots_collection.find_one(filter.view());

        if (result)
        {
            return bsonToSnapshot(result->view());
        }
        else
        {
            logger->logMessage(STRING_FORMAT("Snapshot not found: {%1%}", snapshot_id), LogLevel::ERROR);
            return std::nullopt;
        }
    }
    catch (const mongocxx::exception& e)
    {
        logger->logMessage(STRING_FORMAT("Failed to get snapshot: {%1%}", e.what()), LogLevel::ERROR);
        return std::nullopt;
    }
}

std::optional<Snapshot>
MongoDBStorageService::findSnapshotBySearchKey(const std::string& search_key)
{
    try
    {
        auto client = pool_->acquire();
        auto snapshots_collection = (*client)[config_->database_name][config_->snapshots_collection_name];

        auto filter = bsoncxx_builder::document{} << BSON_SEARCH_KEY_FIELD << search_key << bsoncxx_builder::finalize;
        auto result = snapshots_collection.find_one(filter.view());

        if (result)
        {
            logger->logMessage(STRING_FORMAT("Found snapshot by search key: {%1%}", search_key), LogLevel::DEBUG);
            return bsonToSnapshot(result->view());
        }
        else
        {
            logger->logMessage(STRING_FORMAT("Snapshot not found by search key: {%1%}", search_key), LogLevel::DEBUG);
            return std::nullopt;
        }
    }
    catch (const mongocxx::exception& e)
    {
        logger->logMessage(STRING_FORMAT("Failed to find snapshot by search key: {%1%}", e.what()), LogLevel::ERROR);
        return std::nullopt;
    }
}

SnapshotIdRangeResult MongoDBStorageService::listSnapshotIdsInRange(
    const std::chrono::system_clock::time_point& start_time,
    const std::chrono::system_clock::time_point& end_time,
    size_t                                       limit,
    const std::optional<std::string>&            continuation_token)
{
    SnapshotIdRangeResult out;
    if (limit == 0)
    {
        return out;
    }
    try
    {
        auto client = pool_->acquire();
        auto snapshots_collection = (*client)[config_->database_name][config_->snapshots_collection_name];

        // Convert provided local times to UTC and build time range filter on created_at
        auto start_utc = to_utc(start_time);
        auto end_utc = to_utc(end_time);
        auto time_filter = bsoncxx_builder::document{} << MONGO_GTE_OPERATOR << bsoncxx::types::b_date{start_utc} << MONGO_LTE_OPERATOR
                                                       << bsoncxx::types::b_date{end_utc} << bsoncxx_builder::finalize;
        auto filter = bsoncxx_builder::document{} << BSON_CREATED_AT_FIELD << time_filter << bsoncxx_builder::finalize;

        // Apply continuation token by _id if provided
        if (continuation_token && !continuation_token->empty())
        {
            auto and_filter = bsoncxx_builder::array{};
            and_filter << filter.view();
            and_filter << (bsoncxx_builder::document{} << "_id" << (bsoncxx_builder::document{} << "$gt" << *continuation_token << bsoncxx_builder::finalize)
                                                       << bsoncxx_builder::finalize);
            filter = bsoncxx_builder::document{} << "$and" << and_filter << bsoncxx_builder::finalize;
        }

        mongocxx::options::find opts{};
        opts.projection(bsoncxx_builder::document{} << "_id" << 1 << bsoncxx_builder::finalize);
        opts.sort(bsoncxx_builder::document{} << BSON_CREATED_AT_FIELD << 1 << "_id" << 1 << bsoncxx_builder::finalize);
        opts.limit(static_cast<int64_t>(limit + 1));

        auto   cursor = snapshots_collection.find(filter.view(), opts);
        size_t count = 0;
        for (const auto& doc : cursor)
        {
            if (count >= limit)
            {
                out.has_more = true;
                break;
            }
            if (auto id = doc["_id"])
            {
                std::string sid;
                if (id.type() == bsoncxx::type::k_oid)
                    sid = id.get_oid().value.to_string();
                else if (id.type() == bsoncxx::type::k_string)
                    sid = std::string{id.get_string().value};
                if (!sid.empty())
                {
                    out.snapshot_ids.push_back(sid);
                    out.continuation_token = sid;
                    ++count;
                }
            }
        }

        return out;
    }
    catch (const mongocxx::exception& e)
    {
        logger->logMessage(STRING_FORMAT("Failed to list snapshot IDs in range: {%1%}", e.what()), LogLevel::ERROR);
        return out;
    }
}

bsoncxx::document::value
MongoDBStorageService::snapshotToBson(const Snapshot& snapshot)
{
    auto doc = bsoncxx_builder::document{};

    // Store snapshot creation time in UTC
    if (!snapshot.snapshot_id.empty())
    {
        doc << "_id" << snapshot.snapshot_id;
    }
    doc << BSON_SNAPSHOT_NAME_FIELD << snapshot.snapshot_name
        << BSON_CREATED_AT_FIELD << bsoncxx::types::b_date{to_utc(snapshot.created_at)}
        << BSON_DESCRIPTION_FIELD << snapshot.description
        << BSON_SEARCH_KEY_FIELD << snapshot.search_key;

    // Intentionally omit pv_names field to reduce document size and avoid storing empty arrays

    return doc << bsoncxx_builder::finalize;
}

Snapshot
MongoDBStorageService::bsonToSnapshot(const bsoncxx::document::view& doc)
{
    Snapshot snapshot;

    if (auto id = doc["_id"])
    {
        snapshot.snapshot_id = id.get_string().value;
    }

    if (auto name = doc[BSON_SNAPSHOT_NAME_FIELD])
    {
        snapshot.snapshot_name = name.get_string().value;
    }

    if (auto created_at = doc[BSON_CREATED_AT_FIELD])
    {
        snapshot.created_at = std::chrono::system_clock::time_point{std::chrono::milliseconds{created_at.get_date().value.count()}};
    }

    if (auto description = doc[BSON_DESCRIPTION_FIELD])
    {
        snapshot.description = description.get_string().value;
    }

    if (auto search_key = doc[BSON_SEARCH_KEY_FIELD])
    {
        snapshot.search_key = search_key.get_string().value;
    }

    if (auto pv_names = doc[BSON_PV_NAMES_FIELD])
    {
        auto pv_names_array = pv_names.get_array().value;
        for (const auto& pv_name : pv_names_array)
        {
            if (pv_name.type() == bsoncxx::type::k_string)
            {
                snapshot.pv_names.insert(std::string{pv_name.get_string().value});
            }
        }
    }

    return snapshot;
}
