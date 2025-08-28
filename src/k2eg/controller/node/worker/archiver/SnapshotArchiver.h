#ifndef K2EG_CONTROLLER_NODE_WORKER_ARCHIVER_SNAPSHOTARCHIVER_H_
#define K2EG_CONTROLLER_NODE_WORKER_ARCHIVER_SNAPSHOTARCHIVER_H_

#include <k2eg/common/types.h>
#include <k2eg/controller/node/worker/archiver/BaseArchiver.h>
#include <unordered_map>

namespace k2eg::controller::node::worker::archiver {

/**
 * @brief Constructs a new SnapshotArchiver object.
 * @details This class is responsible for archiving snapshots of the system state.
 * It starts consuming messages from a message queue and processes them to create
 * snapshots that can be stored and retrieved later.
 */
class SnapshotArchiver : public BaseArchiver
{
    // Keep a backlog of fetched messages across performWork() calls
    // so we can continue processing previously fetched data before
    // asking the subscriber for a new batch.
    k2eg::service::pubsub::SubscriberInterfaceElementVector pending_messages;

    /**
     * @brief Parses a snapshot message and extracts relevant information.
     * @param m The snapshot message to parse.
     * @param ser The serialization type to use.
     * @param message_type The extracted message type.
     * @param iter_index The extracted iteration index.
     * @param payload_ts The extracted payload timestamp.
     */
    void parseSnapshotMessage(const service::pubsub::SubscriberInterfaceElement& m,
                              k2eg::common::SerializationType&                   ser,
                              int&                                               message_type,
                              int64_t&                                           iter_index,
                              int64_t&                                           payload_ts,
                              int64_t&                                           header_timestamp,
                              std::string&                                       snapshot_name,
                              std::string&                                       pv_name);

    // Process a single message: build record, manage snapshot lifecycle,
    // store it, and commit on success. Updates created_snapshots cache.
    void processMessage(const service::pubsub::SubscriberInterfaceElement& m,
                        std::unordered_map<std::string, std::string>&      created_snapshots);

public:
    /**
     * @brief Constructs a new SnapshotArchiver object.
     * @param params The configuration for the storage worker.
     * @param subscriber The subscriber to be used for consuming snapshot messages.
     * @param storage_service The storage service to be used for archiving.
     * @param snapshot_queue_name The name of the message queue to consume snapshots from.
     */
    SnapshotArchiver(
        const ArchiverParameters&                      params,
        k2eg::service::log::ILoggerShrdPtr             logger,
        k2eg::service::pubsub::ISubscriberShrdPtr      subscriber,
        k2eg::service::storage::IStorageServiceShrdPtr storage_service);
    /**
     * @brief Destroys the SnapshotArchiver object.
     * @details Cleans up resources and stops any ongoing archiving processes.
     */
    ~SnapshotArchiver();
    /**
     * @brief Performs the work of the SnapshotArchiver.
     * @param timeout The timeout for the work to be performed.
     */
    void performWork(std::chrono::milliseconds timeout) override;
};

DEFINE_PTR_TYPES(SnapshotArchiver)

} // namespace k2eg::controller::node::worker::archiver

#endif // K2EG_CONTROLLER_NODE_WORKER_ARCHIVER_SNAPSHOTARCHIVER_H_
