#ifndef K2EG_CONTROLLER_NODE_WORKER_ARCHIVER_BASEARCHIVER_H_
#define K2EG_CONTROLLER_NODE_WORKER_ARCHIVER_BASEARCHIVER_H_

#include <k2eg/controller/node/worker/StorageWorker.h>

#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/pubsub/ISubscriber.h>
#include <k2eg/service/storage/IStorageService.h>
#include <thread>

namespace k2eg::controller::node::worker::archiver {
/**
 * @brief Base class for archivers in the K2EG controller node worker.
 * @details This class provides a common interface and functionality for all archivers.
 * It can be extended to implement specific archiving strategies.
 */
class BaseArchiver
{
    // Thread for running the archiving process.
    std::thread consumer_thread;
    // Atomic flag to indicate whether archiving is in progress.
    std::atomic<bool> is_archiving{false};

protected:
    ConstStorageWorkerConfigurationShrdPtr config;
    /**
     * @brief Logger for logging messages related to archiving.
     * @details This logger is used to log information, warnings, and errors during the
     * archiving process.
     */
    k2eg::service::log::ILoggerShrdPtr logger;
    /**
     * @brief Subscriber for receiving messages related to archiving.
     * @details This subscriber is used to consume messages from a message queue
     * that contains data to be archived.
     */
    k2eg::service::pubsub::ISubscriberShrdPtr subscriber;

    /**
     * @brief Storage service used for storing archived data.
     * @details This service is responsible for the actual storage implementation
     * (e.g., MongoDB, SQLite) and is injected into the archiver.
     */
    k2eg::service::storage::IStorageServiceShrdPtr storage_service;

    /**
     * @brief Processes messages received from the subscriber.
     * @details This method should be implemented by derived classes to define how
     * messages are processed and archived.
     * @param messages Vector of messages to be processed.
     * @return The number of messages processed.
     */
    virtual int processMessage(service::pubsub::SubscriberInterfaceElementVector& messages) = 0;

public:
    /**
     * @brief Constructs a new BaseArchiver object.
     * @param storage_service_ The storage service to be used for archiving.
     */
    BaseArchiver(ConstStorageWorkerConfigurationShrdPtr config_, k2eg::service::pubsub::ISubscriberShrdPtr subscriber_, k2eg::service::storage::IStorageServiceShrdPtr storage_service_);

    /**
     * @brief Destroys the BaseArchiver object.
     * @details Cleans up resources and stops any ongoing archiving processes.
     */
    virtual ~BaseArchiver();

    /**
     * @brief Starts the archiving process.
     * @details This method should be implemented by derived classes to start archiving.
     */
    virtual void startArchiving();

    /**
     * @brief Stops the archiving process.
     * @details This method should be implemented by derived classes to stop archiving.
     */
    virtual void stopArchiving();

    /**
     * @brief Consumes messages from the subscriber.
     * @details This method fetches messages from the message queue and processes them.
     * It can be overridden by derived classes to implement specific consumption logic.
     * @param data_vector Vector to store the consumed messages.
     * @param m_num Number of messages to consume.
     * @param timeo Timeout for consuming messages.
     */
    void consume();
};

} // namespace k2eg::controller::node::worker::archiver
#endif // K2EG_CONTROLLER_NODE_WORKER_ARCHIVER_BASEARCHIVER_H_