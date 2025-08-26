#ifndef K2EG_CONTROLLER_NODE_WORKER_ARCHIVER_BASEARCHIVER_H_
#define K2EG_CONTROLLER_NODE_WORKER_ARCHIVER_BASEARCHIVER_H_

#include <k2eg/controller/node/worker/StorageWorker.h>

#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/pubsub/ISubscriber.h>
#include <k2eg/service/storage/IStorageService.h>
#include <string>
#include <thread>

namespace k2eg::controller::node::worker::archiver {
/**
 * @brief Initialization parameters for archivers.
 * @details Groups constructor dependencies so they can be passed and stored
 *          consistently and be available to all subclasses via the base class.
 */
struct ArchiverParameters
{
    ConstStorageWorkerConfigurationShrdPtr         config;              ///< Storage worker configuration
    k2eg::service::pubsub::ISubscriberShrdPtr      subscriber;          ///< Input subscriber
    k2eg::service::storage::IStorageServiceShrdPtr storage_service;     ///< Storage backend
    std::string                                    snapshot_queue_name; ///< Optional queue name (used by snapshot archiver)
};

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
    // Aggregated parameters available to all derived archivers
    ArchiverParameters archiver_params;
    // Keep discrete members for current internal usage
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
public:
    /**
     * @brief Constructs a new BaseArchiver object.
     * @param storage_service_ The storage service to be used for archiving.
     */
    BaseArchiver(ConstStorageWorkerConfigurationShrdPtr config_, k2eg::service::pubsub::ISubscriberShrdPtr subscriber_, k2eg::service::storage::IStorageServiceShrdPtr storage_service_);

    /**
     * @brief Constructs a new BaseArchiver object using aggregated parameters.
     */
    explicit BaseArchiver(const ArchiverParameters& params);

    /**
     * @brief Destroys the BaseArchiver object.
     * @details Cleans up resources and stops any ongoing archiving processes.
     */
    virtual ~BaseArchiver();

    /**
     * @brief Performs the work of the archiver.
     * @details This method is called to continue the archiving process.
     * @param timeout The timeout for the work to be performed.
     */
    virtual void performWork(int num_of_msg, int timeout) = 0;

};

} // namespace k2eg::controller::node::worker::archiver
#endif // K2EG_CONTROLLER_NODE_WORKER_ARCHIVER_BASEARCHIVER_H_
