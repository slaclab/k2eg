#include <k2eg/common/utility.h>

#include "k2eg/service/storage/IStorageService.h"
#include <k2eg/service/ServiceResolver.h>
#include <k2eg/service/storage/impl/MongoDBStorageService.h>

#include <k2eg/controller/node/archiver/ArchiverWorker.h>
#include <k2eg/controller/node/worker/StorageWorker.h>

#include <regex>

using namespace k2eg::controller::node::archiver;
using namespace k2eg::controller::node::worker;
using namespace k2eg::service;
using namespace k2eg::service::storage;
using namespace k2eg::service::storage::impl;
using namespace k2eg::service::configuration;
using namespace k2eg::service::log;

ArchiverWorker::ArchiverWorker(const ArchiverConfiguration& config)
    : config_(config)
    , running_(false)
    , logger_(ServiceResolver<ILogger>::resolve())
    , node_config_(ServiceResolver<INodeConfiguration>::resolve())
    , storage_service_(ServiceResolver<IStorageService>::resolve())
{
}

ArchiverWorker::~ArchiverWorker()
{
    if (running_.load())
    {
        stop();
    }
}

bool ArchiverWorker::start()
{
    if (running_.load())
    {
        logger_->logMessage("ArchiverWorker is already running", LogLevel::INFO);
        return false;
    }

    logger_->logMessage("Starting ArchiverWorker...", LogLevel::INFO);

    // Create storage workers based on configuration
    // For now, create one worker per topic group
    size_t worker_count = std::max(size_t(1), static_cast<size_t>(config_.worker_thread_count / 4));

    for (size_t i = 0; i < worker_count; ++i)
    {
        StorageWorkerConfiguration worker_config;
        worker_config.consumer_group_id = config_.consumer_group_id + "_worker_" + std::to_string(i);

        auto worker = std::make_shared<StorageWorker>(worker_config, storage_service_);
        if (!worker->start())
        {
            logger_->logMessage(STRING_FORMAT("Failed to start storage worker {%1%}", i), LogLevel::ERROR);
            return false;
        }

        storage_workers_.push_back(worker);
    }

    running_.store(true);

    // Start topic discovery if enabled
    if (config_.auto_discover_topics)
    {
        topic_discovery_thread_ = std::thread(&ArchiverWorker::topicDiscoveryLoop, this);
    }

    // Start health monitoring
    health_monitor_thread_ = std::thread(&ArchiverWorker::healthMonitorLoop, this);

    logger_->logMessage(STRING_FORMAT("ArchiverWorker started successfully with {%1%} storage workers", storage_workers_.size()), LogLevel::INFO);
    return true;
}

void ArchiverWorker::stop()
{
    if (!running_.load())
    {
        return;
    }

    logger_->logMessage("Stopping ArchiverWorker...", LogLevel::INFO);
    running_.store(false);

    // Stop discovery and health threads
    if (topic_discovery_thread_.joinable())
    {
        topic_discovery_thread_.join();
    }

    if (health_monitor_thread_.joinable())
    {
        health_monitor_thread_.join();
    }

    // Stop all storage workers
    for (auto& worker : storage_workers_)
    {
        worker->stop();
    }
    storage_workers_.clear();

    logger_->logMessage("ArchiverWorker stopped successfully", LogLevel::INFO);
}

void ArchiverWorker::topicDiscoveryLoop()
{
    while (running_.load())
    {
        try
        {
            auto discovered_topics = discoverTopics();
            if (!discovered_topics.empty())
            {
                updateTopicsForWorkers(discovered_topics);
            }
        }
        catch (const std::exception& e)
        {
            logger_->logMessage(STRING_FORMAT("Error in topic discovery: {%1%}", e.what()), LogLevel::ERROR);
        }

        std::this_thread::sleep_for(config_.topic_discovery_interval);
    }
}

void ArchiverWorker::healthMonitorLoop()
{
    while (running_.load())
    {
        try
        {
            // Check storage service health
            if (!storage_service_->isHealthy())
            {
                logger_->logMessage("Storage service is unhealthy", LogLevel::INFO);
            }

            // Check worker health
            for (size_t i = 0; i < storage_workers_.size(); ++i)
            {
                if (!storage_workers_[i]->isRunning())
                {
                    logger_->logMessage(STRING_FORMAT("Storage worker {%1%} is not running", i), LogLevel::INFO);
                    // Could implement restart logic here
                }
            }
        }
        catch (const std::exception& e)
        {
            logger_->logMessage(STRING_FORMAT("Error in health monitoring: {%1%}", e.what()), LogLevel::ERROR);
        }

        std::this_thread::sleep_for(std::chrono::seconds(30));
    }
}

std::vector<std::string> ArchiverWorker::discoverTopics()
{
    std::vector<std::string> topics;

    try
    {
        // This is a simplified implementation
        // In reality, you would query Kafka or Consul for available topics
        // and filter them based on the topic pattern

        std::regex pattern(config_.topic_pattern);

        // Example topics - replace with actual topic discovery
        std::vector<std::string> available_topics = {"epics.pv.data", "epics.monitor.updates", "epics.snapshot.data", "other.unrelated.topic"};

        for (const auto& topic : available_topics)
        {
            if (std::regex_match(topic, pattern))
            {
                topics.push_back(topic);
            }
        }
    }
    catch (const std::exception& e)
    {
        logger_->logMessage(STRING_FORMAT("Error discovering topics: {%1%}", e.what()), LogLevel::ERROR);
    }

    return topics;
}

void ArchiverWorker::updateTopicsForWorkers(const std::vector<std::string>& topics)
{
    if (topics.empty() || storage_workers_.empty())
    {
        return;
    }

    // Distribute topics among workers
    size_t worker_index = 0;
    for (const auto& topic : topics)
    {
        storage_workers_[worker_index]->subscribeToTopics({topic});
        worker_index = (worker_index + 1) % storage_workers_.size();
    }

    logger_->logMessage(STRING_FORMAT("Updated {%1%} topics across {%2%} workers", topics.size() % storage_workers_.size()), LogLevel::INFO);
}

void ArchiverWorker::addTopics(const std::vector<std::string>& topics)
{
    updateTopicsForWorkers(topics);
}

void ArchiverWorker::removeTopics(const std::vector<std::string>& topics)
{
    for (auto& worker : storage_workers_)
    {
        worker->unsubscribeFromTopics(topics);
    }
}
