#include <k2eg/common/BaseSerialization.h>
#include <k2eg/common/utility.h>
#include <k2eg/controller/node/worker/archiver/SnapshotArchiver.h>

#include <chrono>
#include <unordered_map>

using namespace k2eg::service::log;
using namespace k2eg::service::pubsub;
using namespace k2eg::service::storage;

using namespace k2eg::controller::node::worker::archiver;

SnapshotArchiver::SnapshotArchiver(const ArchiverParameters& params)
    : BaseArchiver(params)
{
    logger->logMessage(STRING_FORMAT("SnapshotArchiver started consuming from queue: %1%", archiver_params.snapshot_queue_name), LogLevel::INFO);
    if (!subscriber || !storage_service || !config)
    {
        if (logger)
            logger->logMessage("SnapshotArchiver not properly initialized (subscriber/storage/config missing)", LogLevel::ERROR);
        throw std::runtime_error("SnapshotArchiver not properly initialized (subscriber/storage/config missing)");
    }
}

SnapshotArchiver::~SnapshotArchiver() {}

void SnapshotArchiver::performWork(int /*num_of_msg*/, int timeout)
{
    using namespace std::chrono;
    const auto batch_sz = static_cast<unsigned int>(config->batch_size);

    // Read messages from subscriber (this blocks up to 'timeout')
    service::pubsub::SubscriberInterfaceElementVector messages;
    const int                                         rc = subscriber->getMsg(messages, batch_sz, static_cast<unsigned int>(timeout));
    if (rc != 0 || messages.empty())
    {
        return;
    }

    // Deadline to respect the timeout for this batch
    const auto start_time = steady_clock::now();
    const auto deadline = start_time + milliseconds(timeout);

    // Map to track snapshots created per snapshot_name+iter_index so subsequent messages can reference the id
    std::unordered_map<std::string, std::string> created_snapshots;

    // Process messages individually: create snapshot on header, store each record and commit that message
    size_t processed_count = 0;
    for (const auto& m : messages)
    {
        if (!m)
            continue;

        // If we are very close to deadline, stop processing more messages to avoid overrunning
        if (steady_clock::now() > deadline)
        {
            break;
        }

        // Parse the message payload (encapsulated helper)
        k2eg::common::SerializationType ser;
        int                             message_type;
        int64_t                         iter_index;
        int64_t                         payload_ts;
        int64_t                         header_timestamp;
        std::string                     snapshot_name;
        service::storage::ArchiveRecord rec;

        this->parseSnapshotMessage(*m, ser, message_type, iter_index, payload_ts, header_timestamp, snapshot_name); // Build ArchiveRecord

        rec.pv_name = m->key;
        rec.topic = archiver_params.snapshot_queue_name;

        if (payload_ts > 0)
        {
            rec.timestamp = std::chrono::system_clock::time_point(std::chrono::milliseconds(payload_ts));
        }
        else
        {
            rec.timestamp = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
        }

        try
        {
            boost::json::object meta;
            meta["message_type"] = message_type >= 0 ? message_type : 1;
            if (iter_index >= 0)
                meta["iter_index"] = iter_index;
            if (!snapshot_name.empty())
                meta["snapshot_name"] = snapshot_name;
            rec.metadata = boost::json::serialize(meta);
        }
        catch (...)
        {
            rec.metadata = "";
        }

        // here data is moved
        rec.data = std::make_unique<service::storage::StoredMessage>(m->data.get(), m->data_len, ser);

        // Handle snapshot management: check if snapshot exists, create if needed
        if (!snapshot_name.empty())
        {
            // Use header_timestamp for header messages, or the parsed header_timestamp for data/tail messages
            const int64_t     key_timestamp = (message_type == 0) ? payload_ts : header_timestamp;
            const std::string key = snapshot_name + ":" + std::to_string(key_timestamp) + ":" + std::to_string(iter_index);
            std::string       snapshot_id;

            // First check our local cache
            auto it = created_snapshots.find(key);
            if (it != created_snapshots.end())
            {
                snapshot_id = it->second;
            }
            else
            {
                // Check if snapshot already exists in storage (from previous execution/crash recovery)
                try
                {
                    // Use the efficient search key method instead of listing all snapshots
                    auto existing_snap = storage_service->findSnapshotBySearchKey(key);
                    if (existing_snap.has_value())
                    {
                        // Found a snapshot with the matching search key, use its ID
                        snapshot_id = existing_snap->snapshot_id;
                        created_snapshots[key] = snapshot_id;
                        if (logger)
                            logger->logMessage(STRING_FORMAT("SnapshotArchiver found existing snapshot: %1% for key %2%", snapshot_id % key), LogLevel::DEBUG);
                    }
                }
                catch (const std::exception& ex)
                {
                    if (logger)
                        logger->logMessage(STRING_FORMAT("SnapshotArchiver error checking existing snapshots: %1%", ex.what()), LogLevel::ERROR);
                }

                // If header message and no existing snapshot found, create a new one
                if (snapshot_id.empty() && message_type == 0)
                {
                    service::storage::Snapshot snap;
                    snap.snapshot_name = snapshot_name;
                    snap.created_at = rec.timestamp;
                    snap.description = STRING_FORMAT("Snapshot iteration %1%", iter_index);
                    snap.search_key = key; // Store the search key for efficient lookups
                    try
                    {
                        snapshot_id = storage_service->createSnapshot(snap);
                        if (!snapshot_id.empty())
                        {
                            created_snapshots[key] = snapshot_id;
                            if (logger)
                                logger->logMessage(STRING_FORMAT("SnapshotArchiver created new snapshot: %1% for %2%", snapshot_id % snapshot_name), LogLevel::INFO);
                        }
                    }
                    catch (const std::exception& ex)
                    {
                        if (logger)
                            logger->logMessage(STRING_FORMAT("SnapshotArchiver createSnapshot exception: %1%", ex.what()), LogLevel::ERROR);
                    }
                }
            }

            // Attach snapshot_id to the record if we have one
            if (!snapshot_id.empty())
            {
                rec.snapshot_id = snapshot_id;
            }
        }

        // Store the record immediately (not deferred)
        try
        {
            storage_service->store(rec);
            
            // Only commit after successful storage to prevent duplicates
            if (m->commit_handle)
            {
                try
                {
                    subscriber->commit(m->commit_handle, true);
                }
                catch (const std::exception& ex)
                {
                    if (logger)
                        logger->logMessage(STRING_FORMAT("SnapshotArchiver commit exception: %1%", ex.what()), LogLevel::ERROR);
                }
            }
        }
        catch (const std::exception& ex)
        {
            if (logger)
                logger->logMessage(STRING_FORMAT("SnapshotArchiver store exception: %1%", ex.what()), LogLevel::ERROR);
            // Don't commit if storage failed - message will be redelivered
        }

        // If tail message, we can drop the snapshot mapping for this iteration
        if (message_type == 2 && !snapshot_name.empty())
        {
            const int64_t     key_timestamp = (message_type == 0) ? payload_ts : header_timestamp;
            const std::string key = snapshot_name + ":" + std::to_string(key_timestamp) + ":" + std::to_string(iter_index);
            created_snapshots.erase(key);
        }

        ++processed_count;

        if (processed_count % 50 == 0 && steady_clock::now() > deadline)
        {
            break;
        }
    }
}

void k2eg::controller::node::worker::archiver::SnapshotArchiver::parseSnapshotMessage(const k2eg::service::pubsub::SubscriberInterfaceElement& m,
                                                                                      k2eg::common::SerializationType&                         ser,
                                                                                      int&                                                     message_type,
                                                                                      int64_t&                                                 iter_index,
                                                                                      int64_t&                                                 payload_ts,
                                                                                      int64_t&                                                 header_timestamp,
                                                                                      std::string&                                             snapshot_name)
{
    // Initialize outputs with defaults
    ser = k2eg::common::SerializationType::Unknown;
    message_type = -1;
    iter_index = -1;
    payload_ts = 0;
    header_timestamp = 0;
    snapshot_name.clear();

    // Detect serialization type from header if available
    if (auto it = m.header.find("k2eg-ser-type"); it != m.header.end())
    {
        ser = k2eg::common::serialization_from_string(it->second);
    }

    try
    {
        if (ser == k2eg::common::SerializationType::JSON)
        {
            const char*            dptr = reinterpret_cast<const char*>(m.data.get());
            const std::string_view sv(dptr, m.data_len);
            boost::json::value     jv = boost::json::parse(std::string(sv));
            if (jv.is_object())
            {
                auto& obj = jv.as_object();
                if (obj.contains("message_type"))
                    message_type = static_cast<int>(obj.at("message_type").as_int64());
                if (obj.contains("iter_index"))
                    iter_index = static_cast<int64_t>(obj.at("iter_index").as_int64());
                if (obj.contains("timestamp"))
                    payload_ts = static_cast<int64_t>(obj.at("timestamp").as_int64());
                if (obj.contains("header_timestamp"))
                    header_timestamp = static_cast<int64_t>(obj.at("header_timestamp").as_int64());
                if (obj.contains("snapshot_name"))
                    snapshot_name = obj.at("snapshot_name").as_string();
            }
        }
        else if (ser == k2eg::common::SerializationType::Msgpack || ser == k2eg::common::SerializationType::MsgpackCompact)
        {
            msgpack::object_handle oh = msgpack::unpack(reinterpret_cast<const char*>(m.data.get()), m.data_len);
            msgpack::object        obj = oh.get();
            if (obj.type == msgpack::type::MAP)
            {
                for (uint32_t i = 0; i < obj.via.map.size; ++i)
                {
                    const msgpack::object_kv& kv = obj.via.map.ptr[i];
                    std::string               key = kv.key.as<std::string>();
                    if (key == "message_type")
                        message_type = kv.val.as<int>();
                    else if (key == "iter_index")
                        iter_index = kv.val.as<int64_t>();
                    else if (key == "timestamp")
                        payload_ts = kv.val.as<int64_t>();
                    else if (key == "header_timestamp")
                        header_timestamp = kv.val.as<int64_t>();
                    else if (key == "snapshot_name")
                        snapshot_name = kv.val.as<std::string>();
                }
            }
        }
        else
        {
            // Fallback: try JSON parse
            const char*            dptr = reinterpret_cast<const char*>(m.data.get());
            const std::string_view sv(dptr, m.data_len);
            try
            {
                boost::json::value jv = boost::json::parse(std::string(sv));
                if (jv.is_object())
                {
                    auto& obj = jv.as_object();
                    if (obj.contains("message_type"))
                        message_type = static_cast<int>(obj.at("message_type").as_int64());
                    if (obj.contains("iter_index"))
                        iter_index = static_cast<int64_t>(obj.at("iter_index").as_int64());
                    if (obj.contains("timestamp"))
                        payload_ts = static_cast<int64_t>(obj.at("timestamp").as_int64());
                    if (obj.contains("header_timestamp"))
                        header_timestamp = static_cast<int64_t>(obj.at("header_timestamp").as_int64());
                    if (obj.contains("snapshot_name"))
                        snapshot_name = obj.at("snapshot_name").as_string();
                }
            }
            catch (...)
            { /* ignore */
            }
        }
    }
    catch (const std::exception& /*ex*/)
    {
        // Parsing errors in the helper are ignored; caller may log if needed.
    }
}
