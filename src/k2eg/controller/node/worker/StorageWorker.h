#ifndef K2EG_CONTROLLER_NODE_WORKER_STORAGEWORKER_H_
#define K2EG_CONTROLLER_NODE_WORKER_STORAGEWORKER_H_

#include "k2eg/service/configuration/INodeConfiguration.h"
#include <k2eg/common/BS_thread_pool.hpp>
#include <k2eg/common/types.h>

#include <k2eg/service/configuration/configuration.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/metric/IMetricService.h>
#include <k2eg/service/pubsub/ISubscriber.h>
#include <k2eg/service/scheduler/Task.h>
#include <k2eg/service/storage/IStorageService.h>

#include <boost/program_options.hpp>

#include <atomic>

namespace k2eg::controller::node::worker {

/**
 * @brief Configuration for storage worker
 */
struct StorageWorkerConfiguration
{
    size_t      batch_size = 100;
    size_t      batch_timeout = 100;
    size_t      worker_thread_count = 4;
    size_t      queue_max_size = 10000;
    std::string discover_task_cron = "* * * * * *";         // Every minute
    std::string consumer_group_id = "k2eg-storage-workers"; // Default consumer group

    std::string toString() const
    {
        return std::format("StorageWorkerConfiguration(batch_size={}, batch_timeout={}, worker_thread_count={}, queue_max_size={}, discover_task_cron={}, consumer_group_id={})",
                           batch_size, batch_timeout, worker_thread_count, queue_max_size, discover_task_cron, consumer_group_id);
    }
};

DEFINE_PTR_TYPES(StorageWorkerConfiguration);

/**
 * @brief Fill storage worker program options
 */
void fill_storage_worker_program_option(boost::program_options::options_description& desc);
/**
 * @brief Get storage worker program options from variables map
 *
 * This function extracts storage worker configuration from the provided variables map.
 * It throws an exception if the required section is missing.
 */
ConstStorageWorkerConfigurationShrdPtr get_storage_worker_program_option(const boost::program_options::variables_map& vm);

/**
 * @brief Worker responsible for consuming Kafka messages and storing them
 *
 * This worker consumes EPICS data from Kafka topics and stores them using
 * the configured storage service. It operates in batches for efficiency.
 */
class StorageWorker
{
    // Configuration for the storage worker
    ConstStorageWorkerConfigurationShrdPtr config;
    // Thread pool for managing worker threads
    std::shared_ptr<BS::light_thread_pool> thread_pool;
    // Logger for logging messages related to storage worker operations
    service::log::ILoggerShrdPtr logger;
    // Node configuration service for managing snapshots and node settings
    service::configuration::INodeConfigurationShrdPtr node_config;
    // Storage service for storing consumed data
    k2eg::service::storage::IStorageServiceShrdPtr storage_service;
    // Subscriber for consuming messages from Kafka topics
    k2eg::service::pubsub::ISubscriberShrdPtr subscriber;
    // Metric service for reporting metrics
    k2eg::service::metric::IMetricServiceShrdPtr metric_service;

public:
    StorageWorker(const ConstStorageWorkerConfigurationShrdPtr&, k2eg::service::storage::IStorageServiceShrdPtr);

    ~StorageWorker();

    void executePeriodicTask(k2eg::service::scheduler::TaskProperties& task_properties);
};

DEFINE_PTR_TYPES(StorageWorker)

} // namespace k2eg::controller::node::worker

#endif // K2EG_CONTROLLER_NODE_WORKER_STORAGEWORKER_H_
