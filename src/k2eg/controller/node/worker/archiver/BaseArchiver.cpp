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
    : config(config_)
    , logger(ServiceResolver<ILogger>::resolve())
    , subscriber(subscriber_)
    , storage_service(storage_service_)
{
}

BaseArchiver::~BaseArchiver()
{
    stopArchiving();
}

void BaseArchiver::startArchiving()
{
    // Start consuming messages from the snapshot queue
    // This is where you would set up your message queue consumer
    is_archiving.store(true);

    consumer_thread = std::thread(std::bind(&BaseArchiver::consume, this));
}

void BaseArchiver::stopArchiving()
{
    // Stop consuming messages and clean up resources
    if (!is_archiving.load())
    {
        return;
    }

    // stop the archiver logic here, e.g., stop the message queue consumer
    is_archiving.store(false);
    consumer_thread.join();
}

void BaseArchiver::consume()
{
    // Implementation of the consume method
    int                              c_err = 0;
    SubscriberInterfaceElementVector messages;
    while (is_archiving.load())
    {
        c_err = subscriber->getMsg(messages, config->batch_size, config->batch_timeout);
        if (c_err == 0 && messages.size() > 0)
        {
            if (messages.size() > 0)
            {
                // Process the messages
                processMessage(messages);
            }
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
}