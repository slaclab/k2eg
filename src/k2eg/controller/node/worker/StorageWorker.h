#ifndef K2EG_CONTROLLER_NODE_WORKER_STORAGEWORKER_H_
#define K2EG_CONTROLLER_NODE_WORKER_STORAGEWORKER_H_

#include <k2eg/common/BS_thread_pool.hpp>
#include <k2eg/common/types.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/metric/IMetricService.h>
#include <k2eg/service/pubsub/ISubscriber.h>
#include <k2eg/service/storage/IStorageService.h>

#include <boost/program_options.hpp>

#include <atomic>
#include <thread>

namespace k2eg::controller::node::worker {

/**
 * @brief Configuration for storage worker
 */
struct StorageWorkerConfiguration
{
    size_t batch_size = 100;
    size_t batch_timeout = 100;
    size_t worker_thread_count = 4;
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
    StorageWorkerConfiguration                     config;
    // Logger for logging messages related to storage worker operations
    service::log::ILoggerShrdPtr                   logger;
    // Storage service for storing consumed data
    k2eg::service::storage::IStorageServiceShrdPtr storage_service;
    // Subscriber for consuming messages from Kafka topics
    k2eg::service::pubsub::ISubscriberShrdPtr      subscriber;
    // Metric service for reporting metrics
    k2eg::service::metric::IMetricServiceShrdPtr   metric_service;

    // Thread for checking configuration changes
    std::atomic<bool> running{false};
    std::thread config_checker_thread;

    void configChecker();
public:
    StorageWorker(const StorageWorkerConfiguration&, k2eg::service::storage::IStorageServiceShrdPtr);

    ~StorageWorker();

    /**
     * @brief Start the storage worker
     */
    void start();

    /**
     * @brief Stop the storage worker
     */
    void stop();
};

DEFINE_PTR_TYPES(StorageWorker)

} // namespace k2eg::controller::node::worker

#endif // K2EG_CONTROLLER_NODE_WORKER_STORAGEWORKER_H_
