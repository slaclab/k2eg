#ifndef K2EG_CONTROLLER_NODE_WORKER_ARCHIVER_SNAPSHOTARCHIVER_H_
#define K2EG_CONTROLLER_NODE_WORKER_ARCHIVER_SNAPSHOTARCHIVER_H_

#include <k2eg/controller/node/worker/archiver/BaseArchiver.h>

namespace k2eg::controller::node::worker::archiver {

/**
 * @brief Constructs a new SnapshotArchiver object.
 * @details This class is responsible for archiving snapshots of the system state.
 * It starts consuming messages from a message queue and processes them to create
 * snapshots that can be stored and retrieved later.
 */
class SnapshotArchiver : public BaseArchiver
{
    void parseSnapshotMessage(const service::pubsub::SubscriberInterfaceElement& m,
                              k2eg::common::SerializationType&  ser,
                              int&                              message_type,
                              int64_t&                          iter_index,
                              int64_t&                          payload_ts,
                              std::string&                      snapshot_name);

public:
    /**
     * @brief Constructs a new SnapshotArchiver object.
     * @param config_ The configuration for the storage worker.
     * @param subscriber_ The subscriber to be used for consuming snapshot messages.
     * @param storage_service_ The storage service to be used for archiving.
     * @param snapshot_queue_name_ The name of the message queue to consume snapshots from.
     */
    SnapshotArchiver(const ArchiverParameters& params);
    /**
     * @brief Destroys the SnapshotArchiver object.
     * @details Cleans up resources and stops any ongoing archiving processes.
     */
    ~SnapshotArchiver();
    /**
     * @brief Performs the work of the SnapshotArchiver.
     * @param timeout The timeout for the work to be performed.
     */
    void performWork(int num_of_msg, int timeout) override;
};

} // namespace k2eg::controller::node::worker::archiver

#endif // K2EG_CONTROLLER_NODE_WORKER_ARCHIVER_SNAPSHOTARCHIVER_H_
