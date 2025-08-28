#include <k2eg/common/BaseSerialization.h>
#include <k2eg/common/utility.h>
#include <k2eg/controller/node/worker/StorageWorker.h>
#include <k2eg/controller/node/worker/archiver/SnapshotArchiver.h>

#include <chrono>
#include <unordered_map>

using namespace k2eg::service::log;
using namespace k2eg::service::pubsub;
using namespace k2eg::service::storage;

using namespace k2eg::controller::node::worker::archiver;

SnapshotArchiver::SnapshotArchiver(
    const ArchiverParameters&                      params_,
    k2eg::service::log::ILoggerShrdPtr             logger_,
    k2eg::service::pubsub::ISubscriberShrdPtr      subscriber_,
    k2eg::service::storage::IStorageServiceShrdPtr storage_service_)
    : BaseArchiver(
          params_,
          logger_,
          subscriber_,
          storage_service_)
{
    logger->logMessage(STRING_FORMAT("SnapshotArchiver started consuming from queue: %1%", params.snapshot_queue_name), LogLevel::INFO);
    if (!subscriber || !storage_service)
    {
        if (logger)
            logger->logMessage("SnapshotArchiver not properly initialized (subscriber/storage missing)", LogLevel::ERROR);
        throw std::runtime_error("SnapshotArchiver not properly initialized (subscriber/storage missing)");
    }
    // Ensure the subscriber is subscribed to the snapshot queue
    try
    {
        if (!params_.snapshot_queue_name.empty())
        {
            k2eg::common::StringVector q{params_.snapshot_queue_name};
            subscriber->addQueue(q);
        }
    }
    catch (const std::exception& ex)
    {
        if (logger)
            logger->logMessage(STRING_FORMAT("SnapshotArchiver failed to subscribe to queue: %1%", ex.what()), LogLevel::ERROR);
        throw;
    }
}

SnapshotArchiver::~SnapshotArchiver() {}

void SnapshotArchiver::performWork(std::chrono::milliseconds timeout)
{
    using namespace std::chrono;
    const auto batch_sz = static_cast<unsigned int>(params.engine_config->batch_size);

    // Deadline to respect the timeout for this call (both fetch and process)
    const auto start_time = steady_clock::now();
    const auto deadline = start_time + timeout;

    // Map to track snapshots created per snapshot_name+iter_index so subsequent
    // messages in this invocation can reference the id.
    std::unordered_map<std::string, std::string> created_snapshots;

    // Keep working until we run out of time.
    while (steady_clock::now() < deadline)
    {
        // If no backlog is present, fetch a new batch while respecting the
        // total timeout budget. We allocate most of the time to fetching,
        // but keep a small slice for processing the fetched messages.
        if (pending_messages.empty())
        {
            service::pubsub::SubscriberInterfaceElementVector fetched;

            // Time budget left for this performWork() call.
            auto         now = steady_clock::now();
            milliseconds remaining = duration_cast<milliseconds>(deadline - now);
            if (remaining.count() <= 0)
            {
                // Nothing left in the budget: skip fetch and exit.
                break;
            }

            // Reserve approximately 10% of the remaining time (minimum 10ms)
            // so we can process the fetched messages without exceeding the
            // overall call deadline. Clamp to ensure reservation is always
            // strictly less than the remaining budget when possible.
            milliseconds reserve = remaining / 10; // ~10% of remaining
            if (reserve < milliseconds(10))
                reserve = milliseconds(10);
            if (reserve >= remaining)
                reserve = remaining > milliseconds(1) ? remaining - milliseconds(1) : milliseconds(0);

            // The fetch timeout is whatever remains after the reservation.
            // Ensure we pass a positive timeout (at least 1ms) to the subscriber.
            auto fetch_timeout = remaining - reserve;
            if (fetch_timeout <= milliseconds(0))
                fetch_timeout = milliseconds(1);

            // Fetch up to batch_sz messages using the computed timeout.
            const int rc = subscriber->getMsg(fetched, batch_sz, static_cast<unsigned int>(fetch_timeout.count()));
            if (rc != 0 || fetched.empty())
            {
                // Either no data arrived within the budget or an error occurred.
                // Leave backlog empty and exit; caller will try again on the next tick.
                break;
            }

            // Stash fetched messages as backlog so this invocation can process them.
            pending_messages = std::move(fetched);
        }

        // Process as many pending messages as the remaining time allows.
        size_t processed_count = 0;
        // Process a fixed initial burst without checking time to reduce
        // deadline-check overhead and guarantee some progress even under
        // tight budgets. Keep the burst strictly less than batch size when possible.
        const unsigned int burst_cap = batch_sz > 1 ? (batch_sz - 1) : 1;
        const size_t       burst_target = std::min(pending_messages.size(), static_cast<size_t>(burst_cap));

        for (size_t idx = 0; idx < pending_messages.size(); ++idx)
        {
            // After the initial burst, honor the deadline. This may slightly
            // exceed the budget for the first few messages, which is acceptable.
            if (idx >= burst_target && steady_clock::now() > deadline)
                break;

            const auto& m = pending_messages[idx];
            if (!m)
                continue;

            processMessage(*m, created_snapshots);

            ++processed_count;

            // Periodically re-check the deadline only after the initial burst
            // to avoid excessive time checking overhead.
            if (idx >= burst_target && (processed_count % 50 == 0) && steady_clock::now() > deadline)
            {
                break;
            }
        }

        // Remove the processed messages from the front of the backlog so the
        // next iteration continues with the remaining ones or fetches new data.
        if (processed_count > 0)
        {
            if (processed_count >= pending_messages.size())
                pending_messages.clear();
            else
                pending_messages.erase(pending_messages.begin(), pending_messages.begin() + processed_count);
        }

        // If we couldn't process anything (likely due to timeout), exit.
        if (processed_count == 0)
            break;
    }
}

void SnapshotArchiver::processMessage(const k2eg::service::pubsub::SubscriberInterfaceElement& m,
                                      std::unordered_map<std::string, std::string>&            created_snapshots)
{
    // Parse the message payload (encapsulated helper)
    k2eg::common::SerializationType ser;
    int                             message_type;
    int64_t                         iter_index;
    int64_t                         payload_ts;
    int64_t                         header_timestamp;
    std::string                     snapshot_name;
    std::string pv_name;
    this->parseSnapshotMessage(m, ser, message_type, iter_index, payload_ts, header_timestamp, snapshot_name, pv_name);
    // Compute the iteration key upfront for cache/lookup decisions.
    const int64_t     key_timestamp = (message_type == 0) ? payload_ts : header_timestamp;
    const std::string key = snapshot_name.empty()
                                ? std::string()
                                : (snapshot_name + ":" + std::to_string(key_timestamp) + ":" + std::to_string(iter_index));

    // Fast path for header messages: ensure snapshot existence and commit, skip
    // building ArchiveRecord and metadata to avoid allocations/serialization.
    if (message_type == 0)
    {
        std::string snapshot_id;

        // Reuse context when possible
        if (current_iter.valid && current_iter.key == key && !current_iter.snapshot_id.empty())
        {
            snapshot_id = current_iter.snapshot_id;
        }
        else if (!snapshot_name.empty())
        {
            // Check local per-call cache first
            if (auto it = created_snapshots.find(key); it != created_snapshots.end())
            {
                snapshot_id = it->second;
            }
            else
            {
                // Try storage (recovery or created by another node)
                try
                {
                    auto existing_snap = storage_service->findSnapshotBySearchKey(key);
                    if (existing_snap.has_value())
                    {
                        snapshot_id = existing_snap->snapshot_id;
                        created_snapshots[key] = snapshot_id;
                        if (logger)
                            logger->logMessage(
                                STRING_FORMAT("SnapshotArchiver found existing snapshot: %1% for key %2%", snapshot_id % key),
                                LogLevel::DEBUG);
                    }
                }
                catch (const std::exception& ex)
                {
                    if (logger)
                        logger->logMessage(
                            STRING_FORMAT("SnapshotArchiver error checking existing snapshots: %1%", ex.what()), LogLevel::ERROR);
                }
            }

            // Create if still missing
            if (snapshot_id.empty())
            {
                service::storage::Snapshot snap;
                snap.snapshot_name = snapshot_name;
                snap.created_at = (payload_ts > 0)
                                      ? std::chrono::system_clock::time_point(std::chrono::milliseconds(payload_ts))
                                      : std::chrono::time_point_cast<std::chrono::milliseconds>(
                                            std::chrono::system_clock::now());
                snap.search_key = key;
                try
                {
                    snapshot_id = storage_service->createSnapshot(snap);
                    if (!snapshot_id.empty())
                    {
                        created_snapshots[key] = snapshot_id;
                        if (logger)
                            logger->logMessage(
                                STRING_FORMAT("SnapshotArchiver created new snapshot: %1% for %2%", snapshot_id % snapshot_name),
                                LogLevel::INFO);
                    }
                }
                catch (const std::exception& ex)
                {
                    if (logger)
                        logger->logMessage(STRING_FORMAT("SnapshotArchiver createSnapshot exception: %1%", ex.what()), LogLevel::ERROR);
                }
            }
        }

        // Update iteration context
        if (!snapshot_name.empty() && !key.empty() && !snapshot_id.empty())
        {
            current_iter.valid = true;
            current_iter.key = key;
            current_iter.snapshot_id = snapshot_id;
            current_iter.snapshot_name = snapshot_name;
            current_iter.iter_index = iter_index;
            current_iter.key_timestamp = key_timestamp;
        }

        // Commit header regardless of whether snapshot creation succeeded; data
        // messages will be re-delivered if needed.
        if (m.commit_handle)
        {
            try
            {
                subscriber->commit(m.commit_handle, true);
            }
            catch (const std::exception& ex)
            {
                if (logger)
                    logger->logMessage(STRING_FORMAT("SnapshotArchiver commit exception (header): %1%", ex.what()), LogLevel::ERROR);
            }
        }
        return;
    }

    // Determine snapshot id for data/tail with preference to current iteration
    // context; fall back to caches/storage if context does not match.
    std::string snapshot_id;
    if (current_iter.valid && current_iter.key == key && !current_iter.snapshot_id.empty())
    {
        snapshot_id = current_iter.snapshot_id;
    }
    else if (!snapshot_name.empty())
    {
        if (auto it = created_snapshots.find(key); it != created_snapshots.end())
        {
            snapshot_id = it->second;
            // update context to help subsequent messages if same iteration resumes
            current_iter.valid = true;
            current_iter.key = key;
            current_iter.snapshot_id = snapshot_id;
            current_iter.snapshot_name = snapshot_name;
            current_iter.iter_index = iter_index;
            current_iter.key_timestamp = key_timestamp;
        }
        else
        {
            try
            {
                auto existing_snap = storage_service->findSnapshotBySearchKey(key);
                if (existing_snap.has_value())
                {
                    snapshot_id = existing_snap->snapshot_id;
                    created_snapshots[key] = snapshot_id;
                }
            }
            catch (const std::exception& ex)
            {
                if (logger)
                    logger->logMessage(STRING_FORMAT("SnapshotArchiver error checking existing snapshots: %1%", ex.what()), LogLevel::ERROR);
            }

            if (!snapshot_id.empty())
            {
                current_iter.valid = true;
                current_iter.key = key;
                current_iter.snapshot_id = snapshot_id;
                current_iter.snapshot_name = snapshot_name;
                current_iter.iter_index = iter_index;
                current_iter.key_timestamp = key_timestamp;
            }
        }
    }

    // If we are processing a non-header message (data/tail) and we do not have
    // a snapshot created/found yet, discard the message.
    if (snapshot_id.empty())
    {
        if (logger && !snapshot_name.empty())
            logger->logMessage(
                STRING_FORMAT(
                    "SnapshotArchiver discarding message for snapshot '%1%' (key %2%): snapshot not initialized",
                    snapshot_name % key),
                LogLevel::DEBUG);
        if (m.commit_handle)
        {
            try
            {
                subscriber->commit(m.commit_handle, true);
            }
            catch (const std::exception& ex)
            {
                if (logger)
                    logger->logMessage(STRING_FORMAT("SnapshotArchiver commit exception (discard): %1%", ex.what()), LogLevel::ERROR);
            }
        }
        return; // do not store
    }

    // Build ArchiveRecord lazily (we skipped this for header messages)
    service::storage::ArchiveRecord rec;
    // PV name is carried as the remaining top-level key in data messages
    // (besides known metadata keys). Fall back to message key if not found.
    rec.pv_name = pv_name.empty() ? m.key : pv_name;
    rec.topic = params.snapshot_queue_name;
    rec.snapshot_id = snapshot_id;

    if (payload_ts > 0)
        rec.timestamp = std::chrono::system_clock::time_point(std::chrono::milliseconds(payload_ts));
    else
        rec.timestamp = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());

    try
    {
        boost::json::object meta;
        meta["message_type"] = message_type >= 0 ? message_type : 1;
        meta["iter_index"] = iter_index;
        meta["snapshot_name"] = snapshot_name;
        rec.metadata = boost::json::serialize(meta);
    }
    catch (...)
    {
        rec.metadata = "";
    }

    // Move data
    rec.data = std::make_unique<service::storage::StoredMessage>(m.data.get(), m.data_len, ser);

    // Store the record immediately (not deferred)
    try
    {
        storage_service->store(rec);
        // Only commit after successful storage to prevent duplicates
        if (m.commit_handle)
        {
            try
            {
                subscriber->commit(m.commit_handle, true);
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

    // If tail message, we can drop the snapshot mapping/context for this iteration
    if (message_type == 2 && !snapshot_name.empty())
    {
        created_snapshots.erase(key);
        if (current_iter.valid && current_iter.key == key)
            current_iter.reset();
    }
}

void k2eg::controller::node::worker::archiver::SnapshotArchiver::parseSnapshotMessage(
    const k2eg::service::pubsub::SubscriberInterfaceElement& m,
    k2eg::common::SerializationType&                         ser,
    int&                                                     message_type,
    int64_t&                                                 iter_index,
    int64_t&                                                 payload_ts,
    int64_t&                                                 header_timestamp,
    std::string&                                             snapshot_name,
    std::string&                                             pv_name)
{
    // Initialize outputs with defaults
    ser = k2eg::common::SerializationType::Unknown;
    message_type = -1;
    iter_index = -1;
    payload_ts = 0;
    header_timestamp = 0;
    snapshot_name.clear();
    pv_name.clear();

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
                // message_type may appear as "message_type" or "type"
                if (obj.contains("message_type"))
                    message_type = static_cast<int>(obj.at("message_type").as_int64());
                else if (obj.contains("type"))
                    message_type = static_cast<int>(obj.at("type").as_int64());
                if (obj.contains("iter_index"))
                    iter_index = static_cast<int64_t>(obj.at("iter_index").as_int64());
                if (obj.contains("timestamp"))
                    payload_ts = static_cast<int64_t>(obj.at("timestamp").as_int64());
                if (obj.contains("header_timestamp"))
                    header_timestamp = static_cast<int64_t>(obj.at("header_timestamp").as_int64());
                if (obj.contains("snapshot_name"))
                    snapshot_name = obj.at("snapshot_name").as_string();

                // Extract pv_name as the remaining key for data messages
                // Skip known metadata keys
                if (message_type == 1)
                {
                    for (auto& kv : obj)
                    {
                        const std::string& k = kv.key();
                        if (k == "message_type" || k == "type" || k == "iter_index" || k == "timestamp" ||
                            k == "header_timestamp" || k == "snapshot_name" || k == "error" || k == "error_message")
                            continue;
                        pv_name = k;
                        break;
                    }
                }
            }
        }
        else if (ser == k2eg::common::SerializationType::Msgpack || ser == k2eg::common::SerializationType::MsgpackCompact)
        {
            msgpack::object_handle oh = msgpack::unpack(reinterpret_cast<const char*>(m.data.get()), m.data_len);
            msgpack::object        obj = oh.get();
            if (obj.type == msgpack::type::MAP)
            {
                std::string candidate_pv_key;
                // Single pass: read metadata and remember first non-metadata key
                for (uint32_t i = 0; i < obj.via.map.size; ++i)
                {
                    const msgpack::object_kv& kv = obj.via.map.ptr[i];
                    if (kv.key.type != msgpack::type::STR)
                        continue;
                    std::string key(kv.key.via.str.ptr, kv.key.via.str.size);
                    try
                    {
                        if (key == "message_type")
                        {
                            int mt = 0;
                            kv.val.convert(mt);
                            message_type = mt;
                        }
                        else if (key == "type")
                        {
                            int mt = 0;
                            kv.val.convert(mt);
                            message_type = mt;
                        }
                        else if (key == "iter_index")
                        {
                            kv.val.convert(iter_index);
                        }
                        else if (key == "timestamp")
                        {
                            kv.val.convert(payload_ts);
                        }
                        else if (key == "header_timestamp")
                        {
                            kv.val.convert(header_timestamp);
                        }
                        else if (key == "snapshot_name")
                        {
                            kv.val.convert(snapshot_name);
                        }
                        else if (candidate_pv_key.empty() && key != "error" && key != "error_message")
                        {
                            // remember first non-metadata key as potential PV name
                            candidate_pv_key = key;
                        }
                    }
                    catch (...)
                    {
                        // ignore conversion errors for metadata
                    }
                }

                // If this is a data message, accept the candidate PV key
                if (message_type == 1 && pv_name.empty() && !candidate_pv_key.empty())
                {
                    pv_name = std::move(candidate_pv_key);
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
                    else if (obj.contains("type"))
                        message_type = static_cast<int>(obj.at("type").as_int64());
                    if (obj.contains("iter_index"))
                        iter_index = static_cast<int64_t>(obj.at("iter_index").as_int64());
                    if (obj.contains("timestamp"))
                        payload_ts = static_cast<int64_t>(obj.at("timestamp").as_int64());
                    if (obj.contains("header_timestamp"))
                        header_timestamp = static_cast<int64_t>(obj.at("header_timestamp").as_int64());
                    if (obj.contains("snapshot_name"))
                        snapshot_name = obj.at("snapshot_name").as_string();

                    if (message_type == 1)
                    {
                        for (auto& kv : obj)
                        {
                            const std::string& k = kv.key();
                            if (k == "message_type" || k == "type" || k == "iter_index" || k == "timestamp" ||
                                k == "header_timestamp" || k == "snapshot_name" || k == "error" || k == "error_message")
                                continue;
                            pv_name = k;
                            break;
                        }
                    }
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
