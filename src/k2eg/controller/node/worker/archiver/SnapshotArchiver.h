#ifndef K2EG_CONTROLLER_NODE_WORKER_ARCHIVER_SNAPSHOTARCHIVER_H_
#define K2EG_CONTROLLER_NODE_WORKER_ARCHIVER_SNAPSHOTARCHIVER_H_

#include <k2eg/controller/node/worker/archiver/BaseArchiver.h>
#include <string>

namespace k2eg::controller::node::worker::archiver {

/**
 * @brief Constructs a new SnapshotArchiver object.
 * @details This class is responsible for archiving snapshots of the system state.
 * It starts consuming messages from a message queue and processes them to create
 * snapshots that can be stored and retrieved later.
 */
class SnapshotArchiver : public BaseArchiver
{
    std::atomic<bool> is_archiving{false};

public:
    /**
     * @brief Constructs a new SnapshotArchiver object.
     * @param config_ The configuration for the storage worker.
     * @param subscriber_ The subscriber to be used for consuming snapshot messages.
     * @param storage_service_ The storage service to be used for archiving.
     * @param snapshot_queue_name_ The name of the message queue to consume snapshots from.
     */
    SnapshotArchiver(ConstStorageWorkerConfigurationShrdPtr config_, k2eg::service::pubsub::ISubscriberShrdPtr subscriber_, k2eg::service::storage::IStorageServiceShrdPtr storage_service_, const std::string& snapshot_queue_name_);
    /**
     * @brief Destroys the SnapshotArchiver object.
     * @details Cleans up resources and stops any ongoing archiving processes.
     */
    ~SnapshotArchiver();

    void startArchiving() override;
    void stopArchiving() override;

private:
    std::string snapshot_queue_name;

    int processMessage(service::pubsub::SubscriberInterfaceElementVector& messages) override;
};

} // namespace k2eg::controller::node::worker::archiver

#endif // K2EG_CONTROLLER_NODE_WORKER_ARCHIVER_SNAPSHOTARCHIVER_H_