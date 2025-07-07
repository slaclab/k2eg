#ifndef K2EG_CONTROLLER_NODE_WORKER_STORAGEWORKER_H_
#define K2EG_CONTROLLER_NODE_WORKER_STORAGEWORKER_H_

#include <k2eg/common/BS_thread_pool.hpp>
#include <k2eg/common/types.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/metric/IMetricService.h>
#include <k2eg/service/pubsub/ISubscriber.h>
#include <k2eg/service/storage/IStorageService.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace k2eg::controller::node::worker {

/**
 * @brief Configuration for storage worker
 */
struct StorageWorkerConfiguration
{
    size_t                    batch_size = 100;
    std::chrono::milliseconds batch_timeout = std::chrono::milliseconds(5000);
    size_t                    worker_thread_count = 4;
    size_t                    queue_max_size = 10000;
    std::vector<std::string>  topics_to_consume;
    std::string               consumer_group_id = "k2eg-storage-workers";
};

/**
 * @brief Worker responsible for consuming Kafka messages and storing them
 *
 * This worker consumes EPICS data from Kafka topics and stores them using
 * the configured storage service. It operates in batches for efficiency.
 */
class StorageWorker
{
private:
    service::log::ILoggerShrdPtr                   logger;
    StorageWorkerConfiguration                     config_;
    k2eg::service::storage::IStorageServiceShrdPtr storage_service_;
    k2eg::service::pubsub::ISubscriberShrdPtr      subscriber_;
    k2eg::service::log::ILoggerShrdPtr             logger_;
    k2eg::service::metric::IMetricServiceShrdPtr   metric_service_;

    // Threading and queue management
    std::unique_ptr<BS::light_thread_pool>            thread_pool_;
    std::queue<k2eg::service::storage::ArchiveRecord> record_queue_;
    std::mutex                                        queue_mutex_;
    std::condition_variable                           queue_cv_;
    std::atomic<bool>                                 running_;
    std::atomic<bool>                                 shutdown_requested_;

    // Batch processing
    std::thread                                        batch_processor_thread_;
    std::vector<k2eg::service::storage::ArchiveRecord> current_batch_;
    std::chrono::steady_clock::time_point              last_batch_time_;

    // Statistics
    std::atomic<size_t> messages_consumed_;
    std::atomic<size_t> records_stored_;
    std::atomic<size_t> storage_errors_;

    /**
     * @brief Main message processing loop
     */
    void messageProcessingLoop();

    /**
     * @brief Batch processor thread function
     */
    void batchProcessorLoop();

    /**
     * @brief Flush current batch to storage
     */
    void flushBatch();

    /**
     * @brief Check if batch should be flushed based on size or timeout
     */
    bool shouldFlushBatch() const;

public:
    StorageWorker(const StorageWorkerConfiguration& config, k2eg::service::storage::IStorageServiceShrdPtr storage_service);

    ~StorageWorker();

    /**
     * @brief Start the storage worker
     */
    bool start();

    /**
     * @brief Stop the storage worker
     */
    void stop();

    /**
     * @brief Check if the worker is running
     */
    bool isRunning() const
    {
        return running_.load();
    }

    /**
     * @brief Subscribe to additional topics
     */
    void subscribeToTopics(const std::vector<std::string>& topics);

    /**
     * @brief Unsubscribe from topics
     */
    void unsubscribeFromTopics(const std::vector<std::string>& topics);
};

DEFINE_PTR_TYPES(StorageWorker)

} // namespace k2eg::controller::node::worker

#endif // K2EG_CONTROLLER_NODE_WORKER_STORAGEWORKER_H_
