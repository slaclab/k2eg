#ifndef K2EG_CONTROLLER_NODE_ARCHIVER_ARCHIVERWORKER_H_
#define K2EG_CONTROLLER_NODE_ARCHIVER_ARCHIVERWORKER_H_

#include <k2eg/controller/node/worker/StorageWorker.h>
#include <k2eg/service/storage/IStorageService.h>
#include <k2eg/service/configuration/INodeConfiguration.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/common/types.h>

#include <vector>
#include <atomic>
#include <thread>

namespace k2eg::controller::node::archiver {

/**
 * @brief Configuration for archiver worker
 */
struct ArchiverConfiguration {
    std::string consumer_group_id = "k2eg-archiver"; // Consumer group ID for Kafka
    size_t worker_thread_count = 4; // Number of threads for storage workers
    std::chrono::seconds topic_discovery_interval = std::chrono::seconds(30);
    bool auto_discover_topics = true;
    std::string topic_pattern = "epics.*"; // Regex pattern for topic discovery
};

/**
 * @brief High-level archiver worker that orchestrates storage workers
 * 
 * This worker manages multiple storage workers and handles topic discovery,
 * health monitoring, and coordination between different storage backends.
 */
class ArchiverWorker {
private:
    ArchiverConfiguration config_;
    k2eg::service::storage::IStorageServiceShrdPtr storage_service_;
    k2eg::service::log::ILoggerShrdPtr logger_;
    k2eg::service::configuration::INodeConfigurationShrdPtr node_config_;
    
    std::vector<k2eg::controller::node::worker::StorageWorkerShrdPtr> storage_workers_;
    
    std::atomic<bool> running_;
    std::thread topic_discovery_thread_;
    std::thread health_monitor_thread_;
    
    /**
     * @brief Create storage service based on configuration
     */
    k2eg::service::storage::IStorageServiceShrdPtr createStorageService();
    
    /**
     * @brief Topic discovery loop
     */
    void topicDiscoveryLoop();
    
    /**
     * @brief Health monitoring loop
     */
    void healthMonitorLoop();
    
    /**
     * @brief Discover new topics from Kafka/Consul
     */
    std::vector<std::string> discoverTopics();
    
    /**
     * @brief Update storage workers with new topics
     */
    void updateTopicsForWorkers(const std::vector<std::string>& topics);

public:
    ArchiverWorker(const ArchiverConfiguration& config);
    ~ArchiverWorker();

    /**
     * @brief Start the archiver worker
     */
    bool start();
    
    /**
     * @brief Stop the archiver worker
     */
    void stop();
    
    /**
     * @brief Check if worker is running
     */
    bool isRunning() const { return running_.load(); }
    
    /**
     * @brief Get archiver statistics
     */
    std::string getStatistics() const;
    
    /**
     * @brief Add topics manually
     */
    void addTopics(const std::vector<std::string>& topics);
    
    /**
     * @brief Remove topics
     */
    void removeTopics(const std::vector<std::string>& topics);
};

DEFINE_PTR_TYPES(ArchiverWorker)

} // namespace k2eg::controller::node::archiver

#endif // K2EG_CONTROLLER_NODE_ARCHIVER_ARCHIVERWORKER_H_