#include "k2eg/service/log/ILogger.h"
#include <chrono>
#include <functional>
#include <k2eg/controller/node/worker/archiver/BaseArchiver.h>
#include <k2eg/service/ServiceResolver.h>

using namespace k2eg::controller::node::worker::archiver;

using namespace k2eg::service;
using namespace k2eg::service::log;
using namespace k2eg::service::pubsub;

BaseArchiver::BaseArchiver(ConstStorageWorkerConfigurationShrdPtr config_, k2eg::service::pubsub::ISubscriberShrdPtr subscriber_, k2eg::service::storage::IStorageServiceShrdPtr storage_service_)
    : archiver_params{config_, subscriber_, storage_service_, {}}
    , config(config_)
    , logger(ServiceResolver<ILogger>::resolve())
    , subscriber(subscriber_)
    , storage_service(storage_service_)
{
}

BaseArchiver::BaseArchiver(const ArchiverParameters& params)
    : archiver_params(params)
    , config(params.config)
    , logger(ServiceResolver<ILogger>::resolve())
    , subscriber(params.subscriber)
    , storage_service(params.storage_service)
{
        // Start consuming messages from the snapshot queue
    // This is where you would set up your message queue consumer
    is_archiving.store(true);
}

BaseArchiver::~BaseArchiver()
{

    // stop the archiver logic here, e.g., stop the message queue consumer
    is_archiving.store(false);
}
