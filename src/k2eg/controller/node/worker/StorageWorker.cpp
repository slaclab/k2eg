#include "k2eg/service/pubsub/ISubscriber.h"
#include <cstddef>
#include <k2eg/common/BaseSerialization.h>
#include <k2eg/common/utility.h>
#include <k2eg/service/scheduler/Task.h>

#include <k2eg/service/ServiceResolver.h>
#include <k2eg/service/metric/IMetricService.h>
#include <k2eg/service/scheduler/Scheduler.h>

#include <k2eg/controller/node/worker/StorageWorker.h>
#include <k2eg/controller/node/worker/archiver/SnapshotArchiver.h>

#include <functional>

using namespace k2eg::common;

using namespace k2eg::service;
using namespace k2eg::service::log;
using namespace k2eg::service::metric;
using namespace k2eg::service::pubsub;
using namespace k2eg::service::storage;
using namespace k2eg::service::scheduler;
using namespace k2eg::service::configuration;

using namespace k2eg::controller::node::worker;
using namespace k2eg::controller::node::worker::archiver;

// Default values for StorageWorker configuration
#define DEFAULT_BATCH_SIZE 10
#define DEFAULT_BATCH_TIMEOUT_MS 5000
#define DEFAULT_WORKER_THREAD_COUNT 4
#define DEFAULT_QUEUE_MAX_SIZE 10000
#define DEFAULT_DISCOVER_TASK_CRON "* * * * * *" // Every minute
#define DEFAULT_CONSUMER_GROUP_ID "k2eg-storage-workers"

// Storage Worker Program Option Keys
#define STORAGE_WORKER_SECTION_KEY "Storage Worker"
#define STORAGE_WORKER_BATCH_SIZE_KEY "storage-worker-batch-size"
#define STORAGE_WORKER_BATCH_TIMEOUT_KEY "storage-worker-batch-timeout"
#define STORAGE_WORKER_THREAD_COUNT_KEY "storage-worker-thread-count"
#define STORAGE_WORKER_QUEUE_MAX_SIZE_KEY "storage-worker-queue-max-size"
#define STORAGE_WORKER_DISCOVER_TASK_CRON "storage-worker-discover-task-cron"
#define STORAGE_WORKER_CONSUMER_GROUP_ID_KEY "storage-worker-consumer-group-id"

namespace k2eg::controller::node::worker {

#pragma region Program Options

void fill_storage_worker_program_option(boost::program_options::options_description& desc)
{
    // Create a dedicated section for StorageWorkerConfiguration options
    boost::program_options::options_description storage_worker_section(STORAGE_WORKER_SECTION_KEY);
    // clang-format off
    storage_worker_section.add_options()
        (STORAGE_WORKER_BATCH_SIZE_KEY, boost::program_options::value<int>()->default_value(DEFAULT_BATCH_SIZE), "Batch size for processing")
        (STORAGE_WORKER_BATCH_TIMEOUT_KEY, boost::program_options::value<int>()->default_value(DEFAULT_BATCH_TIMEOUT_MS), "Batch processing timeout in milliseconds")
        (STORAGE_WORKER_THREAD_COUNT_KEY, boost::program_options::value<int>()->default_value(DEFAULT_WORKER_THREAD_COUNT), "Number of worker threads")
        (STORAGE_WORKER_QUEUE_MAX_SIZE_KEY, boost::program_options::value<int>()->default_value(DEFAULT_QUEUE_MAX_SIZE), "Maximum size of the processing queue")
        (STORAGE_WORKER_DISCOVER_TASK_CRON, boost::program_options::value<std::string>()->default_value(DEFAULT_DISCOVER_TASK_CRON), "Cron schedule for the discovery task")
        (STORAGE_WORKER_CONSUMER_GROUP_ID_KEY, boost::program_options::value<std::string>()->default_value(DEFAULT_CONSUMER_GROUP_ID), "Consumer group ID for the storage worker");
    // clang-format on

    // Add the MongoDB section to the main description
    desc.add(storage_worker_section);
}

ConstStorageWorkerConfigurationShrdPtr get_storage_worker_program_option(const boost::program_options::variables_map& vm)
{
    auto config = std::make_shared<StorageWorkerConfiguration>();

    // Extract batch size
    if (vm.count(STORAGE_WORKER_BATCH_SIZE_KEY))
    {
        config->batch_size = vm[STORAGE_WORKER_BATCH_SIZE_KEY].as<int>();
    }
    else
    {
        config->batch_size = DEFAULT_BATCH_SIZE;
    }

    // Extract batch timeout
    if (vm.count(STORAGE_WORKER_BATCH_TIMEOUT_KEY))
    {
        config->batch_timeout = vm[STORAGE_WORKER_BATCH_TIMEOUT_KEY].as<int>();
    }
    else
    {
        config->batch_timeout = DEFAULT_BATCH_TIMEOUT_MS;
    }

    // Extract worker thread count
    if (vm.count(STORAGE_WORKER_THREAD_COUNT_KEY))
    {
        config->worker_thread_count = vm[STORAGE_WORKER_THREAD_COUNT_KEY].as<int>();
    }
    else
    {
        config->worker_thread_count = DEFAULT_WORKER_THREAD_COUNT;
    }
    // Extract queue max size
    if (vm.count(STORAGE_WORKER_QUEUE_MAX_SIZE_KEY))
    {
        config->queue_max_size = vm[STORAGE_WORKER_QUEUE_MAX_SIZE_KEY].as<int>();
    }
    else
    {
        config->queue_max_size = DEFAULT_QUEUE_MAX_SIZE;
    }
    // Extract discover task cron
    if (vm.count(STORAGE_WORKER_DISCOVER_TASK_CRON))
    {
        config->discover_task_cron = vm[STORAGE_WORKER_DISCOVER_TASK_CRON].as<std::string>();
    }
    else
    {
        config->discover_task_cron = DEFAULT_DISCOVER_TASK_CRON;
    }
    // Extract consumer group ID
    if (vm.count(STORAGE_WORKER_CONSUMER_GROUP_ID_KEY))
    {
        config->consumer_group_id = vm[STORAGE_WORKER_CONSUMER_GROUP_ID_KEY].as<std::string>();
    }
    else
    {
        config->consumer_group_id = DEFAULT_CONSUMER_GROUP_ID;
    }

    return config;
}
} // namespace k2eg::controller::node::worker

#pragma region Implementation

#define DISCOVER_TASK_NAME                  "maintenance-task"
#define DISCOVER_TASK_NAME_DEFAULT_CRON     "* * * * * *"

StorageWorker::StorageWorker(const ConstStorageWorkerConfigurationShrdPtr& config_, IStorageServiceShrdPtr storage_service_)
    : logger(ServiceResolver<ILogger>::resolve()), config(config_), thread_pool(std::make_shared<BS::light_thread_pool>(config->worker_thread_count)), storage_service(storage_service_)
{
    // Resolve required services
    logger = ServiceResolver<ILogger>::resolve();
    logger->logMessage("Initializing StorageWorker...", LogLevel::INFO);

    if (!storage_service)
    {
        logger->logMessage("Storage service is not available", LogLevel::ERROR);
        throw std::runtime_error("Storage service is not available");
    }

    node_config = ServiceResolver<INodeConfiguration>::resolve();
    if (!node_config)
    {
        logger->logMessage("Node configuration is not available", LogLevel::ERROR);
        throw std::runtime_error("Node configuration is not available");
    }

    auto task_restart_monitor = MakeTaskShrdPtr(
        DISCOVER_TASK_NAME,
        DISCOVER_TASK_NAME_DEFAULT_CRON,
        std::bind(&StorageWorker::executePeriodicTask, this, std::placeholders::_1),
        -1 // start at application boot time
    );
    ServiceResolver<Scheduler>::resolve()->addTask(task_restart_monitor);

    logger->logMessage(
        STRING_FORMAT("StorageWorker initialized with configuration: %1%", config->toString()),
        LogLevel::INFO);
}

StorageWorker::~StorageWorker()
{
    stop = true;
    logger->logMessage("Destroying StorageWorker...", LogLevel::INFO);
    logger->logMessage("Remove periodic task from scheduler");
    bool erased = ServiceResolver<Scheduler>::resolve()->removeTaskByName(DISCOVER_TASK_NAME);
    logger->logMessage(STRING_FORMAT("Removed periodic discover task result: %1%", erased), LogLevel::DEBUG);
    logger->logMessage("Stopping archivers", LogLevel::DEBUG);
    thread_pool->wait();
    logger->logMessage("StorageWorker destroyed", LogLevel::INFO);
}

void StorageWorker::executePeriodicTask(TaskProperties& task_properties)
{
    logger->logMessage("Check if there are some snapshot to acquire", LogLevel::DEBUG);

    // get all running snapshot that have been requested to be archived but are not associated to any archiver
    auto available_snapshots = node_config->getRunningSnapshotToArchive();
    for (const auto& snapshot_id : available_snapshots)
    {
        auto snapshot = node_config->isSnapshotArchiveRequested(snapshot_id);
        if (!snapshot)
        {
            logger->logMessage(STRING_FORMAT("Snapshot '%1%' is not requested to be archived, skipping acquisition", snapshot_id), LogLevel::DEBUG);
            continue;
        }
        logger->logMessage(STRING_FORMAT("Try to acquire snapshot: %1%", snapshot_id), LogLevel::DEBUG);
        if (node_config->tryAcquireSnapshot(snapshot_id, false)) // false for storage
        {
            logger->logMessage(STRING_FORMAT("Acquired snapshot: %1%", snapshot_id), LogLevel::INFO);

            thread_pool->detach_task(
                [this, snapshot_id]() mutable
                {
                    try
                    {
                        auto archiver = archiver::MakeSnapshotArchiverShrdPtr(
                            ArchiverParameters{
                                .engine_config = this->config,
                                .snapshot_queue_name = snapshot_id},
                            this->logger,
                            ServiceResolver<ISubscriber>::createNewInstance({{"group.id", this->config->consumer_group_id}}),
                            this->storage_service);
                        // set snapshot as archiving
                        node_config->setSnapshotArchiveStatus(snapshot_id, ArchiveStatusInfo{ArchiveStatus::ARCHIVING});
                        // increment running archivers count
                        this->running_archivers.fetch_add(1, std::memory_order_relaxed);
                        // start processing the acquired snapshot
                        processArchiver(archiver);
                    }
                    catch (...)
                    {
                        logger->logMessage(STRING_FORMAT("Failed to create archiver for snapshot: %1%", snapshot_id), LogLevel::ERROR);
                    }
                });
        }
        else
        {
            logger->logMessage(STRING_FORMAT("Failed to acquire snapshot: %1%", snapshot_id), LogLevel::ERROR);
        }
    }

    // Update storage metric: running archivers gauge
    try
    {
        auto metric_service = ServiceResolver<IMetricService>::resolve();
        if (metric_service)
        {
            metric_service->getStorageNodeMetric().incrementCounter(
                IStorageNodeMetricGaugeType::RunningArchivers,
                static_cast<double>(running_archivers.load(std::memory_order_relaxed)));
        }
    }
    catch (...)
    {
        // metrics are optional; ignore failures
    }
}

void StorageWorker::processArchiver(archiver::BaseArchiverShrdPtr archiver)
{
    if (stop)
    {
        // worker shutting down, account for finishing archiver
        running_archivers.fetch_sub(1, std::memory_order_relaxed);
        return;
    }
    // give time to archive
    // logger->logMessage("----------------------------start process archiving---------------------------", LogLevel::DEBUG);
    archiver->performWork(std::chrono::milliseconds(config->batch_timeout));
    // logger->logMessage("----------------------------end process archiving---------------------------", LogLevel::DEBUG);
    if (archiver->canCheckConfig())
    {
        // chek if the snapshot is still running
        auto is_running = node_config->isSnapshotRunning(archiver->params.snapshot_queue_name);
        if (!is_running)
        {
            logger->logMessage(STRING_FORMAT("Snapshot '%1%' is no longer running, stopping archiver", archiver->params.snapshot_queue_name), LogLevel::INFO);
            // set snapshot as not archiving
            node_config->setSnapshotArchiveStatus(archiver->params.snapshot_queue_name, ArchiveStatusInfo{ArchiveStatus::STOPPED});
            // decrement running archivers count
            running_archivers.fetch_sub(1, std::memory_order_relaxed);
            return; // do not resubmit
        }
    }
    // resubmit to scheduler
    thread_pool->detach_task(
        [this, archiver]() mutable
        {
            processArchiver(archiver);
        });
}
