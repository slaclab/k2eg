#ifndef K2EG_CONTROLLER_NODE_WORKER_ARCHIVER_BASEARCHIVER_H_
#define K2EG_CONTROLLER_NODE_WORKER_ARCHIVER_BASEARCHIVER_H_

#include <k2eg/common/types.h>

#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/pubsub/ISubscriber.h>
#include <k2eg/service/storage/IStorageService.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>

// Forward declare to avoid circular include with StorageWorker.h
namespace k2eg::controller::node::worker {
struct StorageWorkerConfiguration;
}

namespace k2eg::controller::node::worker::archiver {
/**
 * @brief Initialization parameters for archivers.
 * @details Groups constructor dependencies so they can be passed and stored
 *          consistently and be available to all subclasses via the base class.
 */
struct ArchiverParameters
{
    std::shared_ptr<const k2eg::controller::node::worker::StorageWorkerConfiguration> engine_config;       ///< Storage worker configuration
    const std::string                                                                 snapshot_queue_name; ///< Name of the queue to consume snapshot messages from.
};

/**
 * @brief Base class for archivers in the controller node.
 * @details Provides common wiring (logger, subscriber, storage) and a minimal
 *          interface for concrete archivers that consume from a queue and
 *          persist records. Subclasses implement performWork() with the
 *          specific consumption/archiving logic.
 */
class BaseArchiver
{
    // Thread for running the archiving process.
    std::thread consumer_thread;
    // Atomic flag to indicate whether archiving is in progress.
    std::atomic<bool> is_archiving{false};

protected:
    // Aggregated parameters available to all derived archivers
    const ArchiverParameters params;
    // Logger for the archiver.
    k2eg::service::log::ILoggerShrdPtr logger;
    // Subscriber for the archiver.
    k2eg::service::pubsub::ISubscriberShrdPtr subscriber;
    // Storage service for the archiver.
    k2eg::service::storage::IStorageServiceShrdPtr storage_service;
public:
    /**
     * @brief Construct a BaseArchiver with required dependencies.
     * @param params Aggregated archiver parameters (engine config, queue name).
     * @param logger Logger instance used by the archiver.
     * @param subscriber Subscriber used to consume messages.
     * @param storage_service Storage service used to persist archived data.
     */
    explicit BaseArchiver(
        const ArchiverParameters&                      params,
        k2eg::service::log::ILoggerShrdPtr             logger,
        k2eg::service::pubsub::ISubscriberShrdPtr      subscriber,
        k2eg::service::storage::IStorageServiceShrdPtr storage_service);

    /**
     * @brief Destroys the BaseArchiver object.
     * @details Cleans up resources and stops any ongoing archiving processes.
     */
    virtual ~BaseArchiver();

    /**
     * @brief Perform a unit of work for the archiver.
     * @details Called repeatedly by the owner to advance consumption and
     *          storage; implement non-blocking or bounded-blocking behavior
     *          respecting the provided timeout.
     * @param timeout Maximum time to wait for new work before returning.
     */
    virtual void performWork(std::chrono::milliseconds timeout) = 0;
};
DEFINE_PTR_TYPES(BaseArchiver)
} // namespace k2eg::controller::node::worker::archiver
#endif // K2EG_CONTROLLER_NODE_WORKER_ARCHIVER_BASEARCHIVER_H_
