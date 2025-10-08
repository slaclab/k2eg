#include "k2eg/service/log/ILogger.h"
#include <chrono>
#include <functional>
#include <k2eg/controller/node/worker/archiver/BaseArchiver.h>
#include <k2eg/service/ServiceResolver.h>

using namespace k2eg::controller::node::worker::archiver;

using namespace k2eg::service;
using namespace k2eg::service::log;
using namespace k2eg::service::pubsub;

BaseArchiver::BaseArchiver(
    const ArchiverParameters&                      params_,
    k2eg::service::log::ILoggerShrdPtr             logger_,
    k2eg::service::pubsub::ISubscriberShrdPtr      subscriber_,
    k2eg::service::storage::IStorageServiceShrdPtr storage_service_)
    : params(params_), logger(logger_), subscriber(subscriber_), storage_service(storage_service_)
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
