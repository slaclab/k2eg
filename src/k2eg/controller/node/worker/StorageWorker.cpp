#include <k2eg/common/BaseSerialization.h>
#include <k2eg/common/utility.h>
#include <k2eg/controller/node/worker/StorageWorker.h>
#include <k2eg/service/ServiceResolver.h>

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
        (BATCH_TIMEOUT_KEY, boost::program_options::value<int>()->default_value(DEFAULT_BATCH_TIMEOUT_MS), "Batch processing timeout in milliseconds")
        (WORKER_THREAD_COUNT_KEY, boost::program_options::value<size_t>()->default_value(DEFAULT_WORKER_THREAD_COUNT), "Number of worker threads")
        (QUEUE_MAX_SIZE_KEY, boost::program_options::value<size_t>()->default_value(DEFAULT_QUEUE_MAX_SIZE), "Maximum size of the message queue")
        (TOPICS_TO_CONSUME_KEY, boost::program_options::value<std::vector<std::string>>()->multitoken(), "List of topics to consume messages from")
        (CONSUMER_GROUP_ID_KEY, boost::program_options::value<std::string>()->default_value(DEFAULT_CONSUMER_GROUP_ID), "Consumer group ID for Kafka");
    // clang-format on

    // Add the MongoDB section to the main description
    desc.add(storage_worker_section);
}

ConstStorageWorkerConfigurationShrdPtr get_storage_worker_program_option(const boost::program_options::variables_map& vm)
{
    auto config = std::make_shared<StorageWorkerConfiguration>();

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
        config->batch_timeout = std::chrono::milliseconds(vm[BATCH_TIMEOUT_KEY].as<int>());
    }
    else
    {
        config->batch_timeout = std::chrono::milliseconds(DEFAULT_BATCH_TIMEOUT_MS);
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

    // Extract queue max size
    if (vm.count(QUEUE_MAX_SIZE_KEY))
    {
        config->queue_max_size = vm[QUEUE_MAX_SIZE_KEY].as<size_t>();
    }
    else
    {
        config->queue_max_size = DEFAULT_QUEUE_MAX_SIZE;
    }

    // Extract topics to consume
    if (vm.count(TOPICS_TO_CONSUME_KEY))
    {
        config->topics_to_consume = vm[TOPICS_TO_CONSUME_KEY].as<std::vector<std::string>>();
    }
    else
    {
        config->topics_to_consume = std::vector<std::string>(); // Empty vector as default
    }

    // Extract consumer group ID
    if (vm.count(CONSUMER_GROUP_ID_KEY))
    {
        config->consumer_group_id = vm[CONSUMER_GROUP_ID_KEY].as<std::string>();
    }
    else
    {
        config->consumer_group_id = DEFAULT_CONSUMER_GROUP_ID;
    }

    return config;
}
} // namespace k2eg::controller::node::worker

#pragma region Implementation
StorageWorker::StorageWorker(const StorageWorkerConfiguration& config, IStorageServiceShrdPtr storage_service)
    : logger(ServiceResolver<ILogger>::resolve()), config_(config), storage_service_(storage_service), running_(false), shutdown_requested_(false), messages_consumed_(0), records_stored_(0), storage_errors_(0), last_batch_time_(std::chrono::steady_clock::now())
{
    // Resolve required services
    logger_ = ServiceResolver<ILogger>::resolve();
    subscriber_ = ServiceResolver<ISubscriber>::resolve();

    // Initialize thread pool
    thread_pool_ = std::make_unique<BS::light_thread_pool>(config_.worker_thread_count);

    // Reserve space for current batch
    current_batch_.reserve(config_.batch_size);
}

StorageWorker::~StorageWorker()
{
    if (running_.load())
    {
        stop();
    }
}

bool StorageWorker::start()
{
    if (running_.load())
    {
        logger->logMessage("StorageWorker is already running", LogLevel::INFO);
        return false;
    }

    running_.store(true);
    shutdown_requested_.store(false);

    // Start batch processor thread
    batch_processor_thread_ = std::thread(&StorageWorker::batchProcessorLoop, this);

    // Start message processing
    // thread_pool_->detach_task(&StorageWorker::messageProcessingLoop, this);

    logger->logMessage(STRING_FORMAT("StorageWorker started successfully with {%1%} worker threads", config_.worker_thread_count), LogLevel::INFO);
    return true;
}

void StorageWorker::stop()
{
    if (!running_.load())
    {
        return;
    }

    logger->logMessage("Stopping StorageWorker...", LogLevel::INFO);
    shutdown_requested_.store(true);

    // Stop subscriber
    // try {
    //     for (const auto& topic : config_.topics_to_consume) {
    //         subscriber_->unsubscribe(topic);
    //     }
    // } catch (const std::exception& e) {
    //     logger->logMessage(STRING_FORMAT("Error during unsubscribe: {%1%}", e.what()), LogLevel::ERROR);
    // }

    // Notify batch processor to wake up and process remaining items
    queue_cv_.notify_all();

    // Wait for batch processor to finish
    if (batch_processor_thread_.joinable())
    {
        batch_processor_thread_.join();
    }

    // Wait for thread pool tasks to complete
    thread_pool_->wait();

    // Flush any remaining batch
    flushBatch();

    running_.store(false);
    logger->logMessage("StorageWorker stopped successfully", LogLevel::INFO);
}

void StorageWorker::messageProcessingLoop()
{
    SubscriberInterfaceElementVector received_message(100);
    while (!shutdown_requested_.load())
    {
        try
        {
            auto messages = subscriber_->getMsg(received_message, 100, 10);

            for (const auto& message : received_message)
            {
                if (shutdown_requested_.load())
                {
                    break;
                }

                // Process message in thread pool
                // thread_pool_->detach_task(&StorageWorker::processMessage, this, message);
                messages_consumed_.fetch_add(1);
            }
        }
        catch (const std::exception& e)
        {
            logger->logMessage(STRING_FORMAT("Error in message processing loop: {%1%}", e.what()), LogLevel::ERROR);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        received_message.clear();
    }
}

void StorageWorker::batchProcessorLoop()
{
    while (!shutdown_requested_.load())
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);

        // Wait for records or timeout
        queue_cv_.wait_for(lock, config_.batch_timeout,
                           [this]
                           {
                               return !record_queue_.empty() || shutdown_requested_.load() || shouldFlushBatch();
                           });

        // Move records from queue to batch
        while (!record_queue_.empty() && current_batch_.size() < config_.batch_size)
        {
            current_batch_.push_back(std::move(record_queue_.front()));
            record_queue_.pop();
        }

        lock.unlock();

        // Flush batch if needed
        if (shouldFlushBatch() || shutdown_requested_.load())
        {
            flushBatch();
        }
    }

    // Final flush
    flushBatch();
}

void StorageWorker::flushBatch()
{
    if (current_batch_.empty())
    {
        return;
    }

    try
    {
        size_t stored_count = storage_service_->storeBatch(current_batch_);
        records_stored_.fetch_add(stored_count);

        if (stored_count != current_batch_.size())
        {
            storage_errors_.fetch_add(current_batch_.size() - stored_count);
            logger->logMessage(
                STRING_FORMAT("Only stored {%1%} out of {%2%} records in batch", stored_count % current_batch_.size()), LogLevel::INFO);
        }

        logger->logMessage(STRING_FORMAT("Flushed batch of {%1%} records", stored_count), LogLevel::DEBUG);
    }
    catch (const std::exception& e)
    {
        logger->logMessage(STRING_FORMAT("Error flushing batch: {%1%}", e.what()), LogLevel::ERROR);
        storage_errors_.fetch_add(current_batch_.size());
    }

    current_batch_.clear();
    last_batch_time_ = std::chrono::steady_clock::now();
}

bool StorageWorker::shouldFlushBatch() const
{
    if (current_batch_.empty())
    {
        return false;
    }

    // Flush if batch is full
    if (current_batch_.size() >= config_.batch_size)
    {
        return true;
    }

    // Flush if timeout reached
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_batch_time_);
    return elapsed >= config_.batch_timeout;
}

void StorageWorker::subscribeToTopics(const std::vector<std::string>& topics)
{
    for (const auto& topic : topics)
    {
        try
        {
            // subscriber_->subscribe(topic, config_.consumer_group_id);
            config_.topics_to_consume.push_back(topic);
            logger->logMessage(STRING_FORMAT("Subscribed to additional topic: {%1%}", topic), LogLevel::INFO);
        }
        catch (const std::exception& e)
        {
            logger->logMessage(STRING_FORMAT("Failed to subscribe to topic {%1%}: {%2%}", topic % e.what()), LogLevel::ERROR);
        }
    }
}

void StorageWorker::unsubscribeFromTopics(const std::vector<std::string>& topics)
{
    for (const auto& topic : topics)
    {
        try
        {
            // subscriber_->unsubscribe(topic);

            // Remove from config
            auto it = std::find(config_.topics_to_consume.begin(), config_.topics_to_consume.end(), topic);
            if (it != config_.topics_to_consume.end())
            {
                config_.topics_to_consume.erase(it);
            }

            logger->logMessage(STRING_FORMAT("Unsubscribed from topic: {%1%}", topic), LogLevel::INFO);
        }
        catch (const std::exception& e)
        {
            logger->logMessage(STRING_FORMAT("Failed to unsubscribe from topic {%1%}: {%2%}", topic % e.what()), LogLevel::ERROR);
        }
    }
}
