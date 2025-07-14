#include "k2eg/controller/node/worker/archiver/BaseArchiver.h"
#include <k2eg/controller/node/worker/archiver/SnapshotArchiver.h>

#include <k2eg/common/utility.h>

using namespace k2eg::service::log;
using namespace k2eg::service::pubsub;
using namespace k2eg::service::storage;

using namespace k2eg::controller::node::worker::archiver;

SnapshotArchiver::SnapshotArchiver(ConstStorageWorkerConfigurationShrdPtr config_, k2eg::service::pubsub::ISubscriberShrdPtr subscriber_, k2eg::service::storage::IStorageServiceShrdPtr storage_service_, const std::string& snapshot_queue_name_)
    : BaseArchiver(config_, subscriber_, storage_service_), snapshot_queue_name(snapshot_queue_name_)
{
}

SnapshotArchiver::~SnapshotArchiver() {}

void SnapshotArchiver::startArchiving()
{
    // Additional logic specific to snapshot archiving can be added here
    BaseArchiver::startArchiving();

    logger->logMessage(STRING_FORMAT("SnapshotArchiver started consuming from queue: %1%", snapshot_queue_name), LogLevel::INFO);
}

void SnapshotArchiver::stopArchiving()
{
    BaseArchiver::stopArchiving();
    // additional logic specific to snapshot archiving can be added here
}

int SnapshotArchiver::processMessage(SubscriberInterfaceElementVector& messages) {
    // TODO: Implement snapshot-specific message processing
    return 0;
}
