#ifndef k2eg_CONTROLLER_NODE_NODECONTROLLER_H_
#define k2eg_CONTROLLER_NODE_NODECONTROLLER_H_

#include <k2eg/common/BS_thread_pool.hpp>
#include <k2eg/common/ObjectFactory.h>
#include <k2eg/common/ProcSystemMetrics.h>
#include <k2eg/common/types.h>

#include <k2eg/service/data/DataStorage.h>
#include <k2eg/service/epics/EpicsServiceManager.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/metric/IMetricService.h>
#include <k2eg/service/metric/INodeControllerMetric.h>
#include <k2eg/service/metric/INodeControllerSystemMetric.h>

#include <k2eg/controller/command/CMDCommand.h>
#include <k2eg/controller/node/configuration/NodeConfiguration.h>
#include <k2eg/controller/node/worker/CommandWorker.h>
#include <k2eg/controller/node/worker/MonitorCommandWorker.h>
#include <k2eg/controller/node/worker/SnapshotCommandWorker.h>

#include <memory>

namespace k2eg::controller::node {

struct NodeControllerConfiguration
{
    // monitor configuration
    worker::monitor::MonitorCommandConfiguration monitor_command_configuration;
    worker::SnapshotCommandConfiguration         snapshot_command_configuration;
};
DEFINE_PTR_TYPES(NodeControllerConfiguration)

/**
 * @brief Main controller class for the node operation.
 *
 * Responsible for managing command workers, handling node configuration,
 * collecting system metrics, and coordinating management tasks.
 */
class NodeController
{
    /// Collects and provides process/system metrics.
    k2eg::common::ProcSystemMetrics proc_system_metrics_grabber;
    /// Node controller configuration (immutable, shared pointer).
    ConstNodeControllerConfigurationUPtr node_controller_configuration;
    /// Thread pool for processing tasks.
    std::shared_ptr<BS::light_thread_pool> processing_pool;
    /// Resolves and creates command workers based on command type.
    k2eg::common::ObjectByTypeFactory<k2eg::controller::command::cmd::CommandType, worker::CommandWorker> worker_resolver;
    /// Shared pointer to node configuration.
    configuration::NodeConfigurationShrdPtr node_configuration;

    /// Logger instance for logging events and errors.
    k2eg::service::log::ILoggerShrdPtr logger;
    /// Manages EPICS service instances.
    k2eg::service::epics_impl::EpicsServiceManager epics_service_manager;
    /// Reference to the node controller metric service.
    k2eg::service::metric::INodeControllerMetric& metrics;
    /// Reference to the node controller system metric service.
    k2eg::service::metric::INodeControllerSystemMetric& system_metrics;

    /**
     * @brief Handles the collection and reporting of statistics for a given task.
     * @param task_properties Properties of the task to process statistics for.
     */
    void handleStatistic(k2eg::service::scheduler::TaskProperties& task_properties);

public:
    /**
     * @brief Constructs a NodeController with the given configuration and data storage.
     * @param node_controller_configuration Node controller configuration (unique pointer).
     * @param data_storage Shared pointer to the data storage service.
     */
    NodeController(ConstNodeControllerConfigurationUPtr node_controller_configuration, k2eg::service::data::DataStorageShrdPtr data_storage);

    /// Deleted default constructor.
    NodeController() = delete;
    /// Deleted copy constructor.
    NodeController(const NodeController&) = delete;
    /// Deleted copy assignment operator.
    NodeController& operator=(const NodeController&) = delete;

    /// Destructor.
    ~NodeController();

    /**
     * @brief Performs periodic management tasks, such as metrics collection and housekeeping.
     */
    void performManagementTask();

    /**
     * @brief Waits for all currently running tasks to complete.
     */
    void waitForTaskCompletion();

    /**
     * @brief Checks if a worker for the given command type is ready.
     * @param cmd_type The command type to check.
     * @return True if the worker is ready, false otherwise.
     */
    bool isWorkerReady(k2eg::controller::command::cmd::CommandType cmd_type);

    /**
     * @brief Gets the number of running tasks for a given command type.
     * @param cmd_type The command type to query.
     * @return The number of running tasks.
     */
    std::size_t getTaskRunning(k2eg::controller::command::cmd::CommandType cmd_type);

    /**
     * @brief Submits an array of commands for processing.
     * @param commands Vector of shared pointers to constant command objects.
     */
    void submitCommand(k2eg::controller::command::cmd::ConstCommandShrdPtrVec commands);
};
DEFINE_PTR_TYPES(NodeController)
} // namespace k2eg::controller::node

#endif // k2eg_CONTROLLER_NODE_NODECONTROLLER_H_