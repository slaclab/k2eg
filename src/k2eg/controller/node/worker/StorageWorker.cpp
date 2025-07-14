#include <functional>
#include <k2eg/common/BaseSerialization.h>
#include <k2eg/common/utility.h>
#include <k2eg/controller/node/worker/StorageWorker.h>
#include <k2eg/service/ServiceResolver.h>
#include <thread>

using namespace k2eg::controller::node::worker;
using namespace k2eg::service;
using namespace k2eg::service::storage;
using namespace k2eg::service::pubsub;
using namespace k2eg::service::log;
using namespace k2eg::service::metric;
using namespace k2eg::common;

// Default values for StorageWorker configuration
#define DEFAULT_BATCH_SIZE 100
#define DEFAULT_BATCH_TIMEOUT_MS 5000
#define DEFAULT_WORKER_THREAD_COUNT 4
#define DEFAULT_QUEUE_MAX_SIZE 10000
#define DEFAULT_CONSUMER_GROUP_ID "k2eg-storage-workers"

// Storage Worker Program Option Keys
#define STORAGE_WORKER_SECTION_KEY "storage-worker"
#define BATCH_SIZE_KEY "batch-size"
#define BATCH_TIMEOUT_KEY "batch-timeout"
#define WORKER_THREAD_COUNT_KEY "worker-thread-count"
#define QUEUE_MAX_SIZE_KEY "queue-max-size"
#define TOPICS_TO_CONSUME_KEY "topics-to-consume"
#define CONSUMER_GROUP_ID_KEY "consumer-group-id"

namespace k2eg::controller::node::worker {

#pragma region Program Options

void fill_storage_worker_program_option(boost::program_options::options_description& desc)
{
    // Create a dedicated section for StorageWorkerConfiguration options
    boost::program_options::options_description storage_worker_section(STORAGE_WORKER_SECTION_KEY);
    // clang-format off
    storage_worker_section.add_options()
        (BATCH_SIZE_KEY, boost::program_options::value<size_t>()->default_value(DEFAULT_BATCH_SIZE), "Batch size for processing")
        (BATCH_TIMEOUT_KEY, boost::program_options::value<size_t>()->default_value(DEFAULT_BATCH_TIMEOUT_MS), "Batch processing timeout in milliseconds")
        (WORKER_THREAD_COUNT_KEY, boost::program_options::value<size_t>()->default_value(DEFAULT_WORKER_THREAD_COUNT), "Number of worker threads");
    // clang-format on

    // Add the MongoDB section to the main description
    desc.add(storage_worker_section);
}

ConstStorageWorkerConfigurationShrdPtr get_storage_worker_program_option(const boost::program_options::variables_map& vm)
{
    auto config = std::make_shared<StorageWorkerConfiguration>();

    if (vm.count(STORAGE_WORKER_SECTION_KEY) == 0)
    {
        return config; // Return default configuration if section is not present
    }

    // Extract batch size
    if (vm.count(BATCH_SIZE_KEY))
    {
        config->batch_size = vm[BATCH_SIZE_KEY].as<size_t>();
    }
    else
    {
        config->batch_size = DEFAULT_BATCH_SIZE;
    }

    // Extract batch timeout
    if (vm.count(BATCH_TIMEOUT_KEY))
    {
        config->batch_timeout = vm[BATCH_TIMEOUT_KEY].as<size_t>();
    }
    else
    {
        config->batch_timeout = DEFAULT_BATCH_TIMEOUT_MS;
    }

    // Extract worker thread count
    if (vm.count(WORKER_THREAD_COUNT_KEY))
    {
        config->worker_thread_count = vm[WORKER_THREAD_COUNT_KEY].as<size_t>();
    }
    else
    {
        config->worker_thread_count = DEFAULT_WORKER_THREAD_COUNT;
    }

    return config;
}
} // namespace k2eg::controller::node::worker

#pragma region Implementation

StorageWorker::StorageWorker(const StorageWorkerConfiguration& config_, IStorageServiceShrdPtr storage_service_)
    : logger(ServiceResolver<ILogger>::resolve()), config(config_), storage_service(storage_service_)
{
    // Resolve required services
    logger = ServiceResolver<ILogger>::resolve();
    subscriber = ServiceResolver<ISubscriber>::resolve();
}

StorageWorker::~StorageWorker()
{
    stop();
}

void StorageWorker::start()
{
    // Initialize thread pool
    config_checker_thread = std::thread(std::bind(&StorageWorker::configChecker, this));
    logger->logMessage("StorageWorker started successfully", LogLevel::INFO);
}

void StorageWorker::stop()
{

    logger->logMessage("StorageWorker stopped successfully", LogLevel::INFO);
}

void StorageWorker::configChecker()
{
    while (running.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        // Check and update configuration if needed
    }
}