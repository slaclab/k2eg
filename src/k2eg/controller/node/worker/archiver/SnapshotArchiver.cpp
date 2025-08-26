#include "k2eg/controller/node/worker/archiver/BaseArchiver.h"
#include <k2eg/controller/node/worker/archiver/SnapshotArchiver.h>

#include <k2eg/common/BaseSerialization.h>
#include <k2eg/common/utility.h>

#include <chrono>
#include <vector>
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
        std::string                     snapshot_name;

    this->parseSnapshotMessage(*m, ser, message_type, iter_index, payload_ts, snapshot_name);

        // Build ArchiveRecord
        service::storage::ArchiveRecord rec;
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
        catch (...) { rec.metadata = ""; }

        rec.data = std::make_unique<service::storage::StoredMessage>(m->data.get(), m->data_len, ser);

        // Handle snapshot management: check if snapshot exists, create if needed
        if (!snapshot_name.empty())
        {
            const std::string key = snapshot_name + ":" + std::to_string(iter_index);
            std::string snapshot_id;
            
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
                    // Query existing snapshots to find one with matching name and created around the same time
                    auto existing_snapshots = storage_service->listSnapshots();
                    for (const auto& existing_snap : existing_snapshots)
                    {
                        if (existing_snap.snapshot_name == snapshot_name)
                        {
                            // Found a snapshot with the same name, use its ID
                            snapshot_id = existing_snap.snapshot_id;
                            created_snapshots[key] = snapshot_id;
                            if (logger)
                                logger->logMessage(STRING_FORMAT("SnapshotArchiver found existing snapshot: %1% for %2%", snapshot_id % snapshot_name), LogLevel::DEBUG);
                            break;
                        }
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

        // Prepare the storage action to be executed on commit
        auto rec_ptr = std::make_shared<service::storage::ArchiveRecord>(std::move(rec));
        // share the snapshot map across lambdas
        auto shared_snapshots = std::make_shared<std::unordered_map<std::string, std::string>>(created_snapshots);

        // Attach on_commit_action to commit handle so storage happens only after commit
        if (m->commit_handle)
        {
            // Need to make a mutable copy of the shared map for lambdas to update
            auto snaps_ptr = std::make_shared<std::unordered_map<std::string, std::string>>();
            // initialize with current map snapshot
            *snaps_ptr = *shared_snapshots;

            const std::string snap_name_copy = snapshot_name;
            const int64_t iter_index_copy = iter_index;
            const int msg_type_copy = message_type;

            const auto storage = storage_service;

            const_cast<SubscriberInterfaceElement::CommitHandle*>(m->commit_handle.get())->on_commit_action = [storage, rec_ptr, snaps_ptr, snap_name_copy, iter_index_copy, msg_type_copy, this]() {
                try
                {
                    // Check if we need to find an existing snapshot first
                    std::string snapshot_id;
                    if (!snap_name_copy.empty())
                    {
                        const std::string key = snap_name_copy + ":" + std::to_string(iter_index_copy);
                        auto it = snaps_ptr->find(key);
                        if (it != snaps_ptr->end())
                        {
                            snapshot_id = it->second;
                        }
                        else
                        {
                            // Check storage for existing snapshot
                            try
                            {
                                auto existing_snapshots = storage->listSnapshots();
                                for (const auto& existing_snap : existing_snapshots)
                                {
                                    if (existing_snap.snapshot_name == snap_name_copy)
                                    {
                                        snapshot_id = existing_snap.snapshot_id;
                                        (*snaps_ptr)[key] = snapshot_id;
                                        break;
                                    }
                                }
                            }
                            catch (...) { /* ignore lookup errors */ }
                            
                            // If header and no existing snapshot, create one
                            if (snapshot_id.empty() && msg_type_copy == 0)
                            {
                                service::storage::Snapshot snap;
                                snap.snapshot_name = snap_name_copy;
                                snap.created_at = rec_ptr->timestamp;
                                snap.description = STRING_FORMAT("Snapshot iteration %1%", iter_index_copy);
                                snapshot_id = storage->createSnapshot(snap);
                                if (!snapshot_id.empty())
                                {
                                    (*snaps_ptr)[key] = snapshot_id;
                                }
                            }
                        }
                        
                        // Attach snapshot_id to record
                        if (!snapshot_id.empty())
                        {
                            rec_ptr->snapshot_id = snapshot_id;
                        }
                    }
                    
                    // Store the record
                    storage->store(*rec_ptr);
                    
                    // If tail, remove mapping
                    if (msg_type_copy == 2 && !snap_name_copy.empty())
                    {
                        const std::string key = snap_name_copy + ":" + std::to_string(iter_index_copy);
                        snaps_ptr->erase(key);
                    }
                }
                catch (const std::exception& ex)
                {
                    if (this->logger)
                        this->logger->logMessage(STRING_FORMAT("SnapshotArchiver on_commit_action exception: %1%", ex.what()), LogLevel::ERROR);
                }
            };

            // Now commit this specific message which will trigger the on_commit_action upon success
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

        // If tail message, we can drop the snapshot mapping for this iteration
        if (message_type == 2 && !snapshot_name.empty())
        {
            const std::string key = snapshot_name + ":" + std::to_string(iter_index);
            created_snapshots.erase(key);
        }

        ++processed_count;

        if (processed_count % 50 == 0 && steady_clock::now() > deadline)
        {
            break;
        }
    }
}

// Helper: parse snapshot message payload to extract serialization and metadata
void k2eg::controller::node::worker::archiver::SnapshotArchiver::parseSnapshotMessage(const k2eg::service::pubsub::SubscriberInterfaceElement& m,
                                                                                         k2eg::common::SerializationType&  ser,
                                                                                         int&                              message_type,
                                                                                         int64_t&                          iter_index,
                                                                                         int64_t&                          payload_ts,
                                                                                         std::string&                      snapshot_name)
{
    // Initialize outputs with defaults
    ser = k2eg::common::SerializationType::Unknown;
    message_type = -1;
    iter_index = -1;
    payload_ts = 0;
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
