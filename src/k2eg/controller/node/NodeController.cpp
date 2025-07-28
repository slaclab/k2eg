#include <k2eg/common/BS_thread_pool.hpp>
#include <k2eg/common/utility.h>

#include <k2eg/service/ServiceResolver.h>
#include <k2eg/service/data/DataStorage.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/metric/INodeControllerMetric.h>

#include <k2eg/controller/node/NodeController.h>
#include <k2eg/controller/node/worker/GetCommandWorker.h>
#include <k2eg/controller/node/worker/MonitorCommandWorker.h>
#include <k2eg/controller/node/worker/PutCommandWorker.h>
#include <k2eg/controller/node/worker/SnapshotCommandWorker.h>

using namespace k2eg::controller::node;
using namespace k2eg::controller::node::worker;
using namespace k2eg::controller::node::worker::monitor;
using namespace k2eg::controller::node::configuration;

using namespace k2eg::controller::command;
using namespace k2eg::controller::command::cmd;

using namespace k2eg::service;
using namespace k2eg::service::data;
using namespace k2eg::service::data::repository;
using namespace k2eg::service::log;
using namespace k2eg::service::epics_impl;
using namespace k2eg::service::metric;
using namespace k2eg::service::scheduler;

#define NODE_CONTROLLER_STAT_TASK_NAME "node-controller-stat-task"
#define NODE_CONTROLLER_STAT_TASK_CRON "* * * * * *"

const std::map<std::string, std::string> submitted_command_labels = {{"op", "command_submitted"}};
const std::map<std::string, std::string> submitted_command_no_workef_found_labels = {{"op", "no_worker_found"}};

NodeController::NodeController(ConstNodeControllerConfigurationUPtr node_controller_configuration, DataStorageShrdPtr data_storage)
    : node_controller_configuration(std::move(node_controller_configuration))
    , node_configuration(std::make_shared<NodeConfiguration>(data_storage))
    , processing_pool(std::make_shared<BS::light_thread_pool>())
    , metrics(ServiceResolver<IMetricService>::resolve()->getNodeControllerMetric())
    , system_metrics(ServiceResolver<IMetricService>::resolve()->getNodeControllerSystemMetric())
    , epics_service_manager_shrd_ptr(ServiceResolver<EpicsServiceManager>::resolve())
{
    // set logger
    logger = ServiceResolver<ILogger>::resolve();

    // load current node configuration
    logger->logMessage(STRING_FORMAT("Starting k2eg %1% node", this->node_controller_configuration->node_type), LogLevel::INFO);

    // check which kind of node controller we are
    switch (this->node_controller_configuration->node_type)
    {
    case NodeType::GATEWAY:
        startAsGateway();
        break;
    case NodeType::STORAGE:
        startAsStorage();
        break;
    default:
        throw std::runtime_error("Unknown node type in NodeController configuration");
    }

    // Add periodic statistics management task to the scheduler.
    // This task is responsible for collecting and reporting system metrics.
    auto statistic_task = MakeTaskShrdPtr(NODE_CONTROLLER_STAT_TASK_NAME,                                           // name of the task
                                          NODE_CONTROLLER_STAT_TASK_CRON,                                           // cron expression
                                          std::bind(&NodeController::handleStatistic, this, std::placeholders::_1), // task handler
                                          -1                                                                        // start at application boot time
    );
    ServiceResolver<Scheduler>::resolve()->addTask(statistic_task);
}

NodeController::~NodeController()
{
    // Remove the periodic statistics task from the scheduler.
    bool erased = ServiceResolver<Scheduler>::resolve()->removeTaskByName(NODE_CONTROLLER_STAT_TASK_NAME);
    logger->logMessage(STRING_FORMAT("Remove statistic task : %1%", erased));
    logger->logMessage("Stopping node controller");
    // Wait for all processing tasks to finish before shutdown.
    processing_pool->wait();
    // Clear all registered workers.
    worker_resolver.clear();
    logger->logMessage("Node controller stopped", LogLevel::INFO);
}

void NodeController::performManagementTask()
{
    switch (this->node_controller_configuration->node_type)
    {
    case NodeType::GATEWAY:
        performGatewayPeriodicTask();
        break;
    case NodeType::STORAGE:
        performStoragePeriodicTask();
        break;
    default:
        throw std::runtime_error("Unknown node type in NodeController configuration");
    }
}

void NodeController::waitForTaskCompletion()
{
    // Wait for all detached tasks in the thread pool to complete.
    processing_pool->wait();
}

bool NodeController::isWorkerReady(k2eg::controller::command::cmd::CommandType cmd_type)
{
    if (auto worker = worker_resolver.resolve(cmd_type); worker != nullptr)
    {
        return worker->isReady();
    }
    else
    {
        return false;
    }
}

std::size_t NodeController::getTaskRunning(k2eg::controller::command::cmd::CommandType cmd_type)
{
    if (auto worker = worker_resolver.resolve(cmd_type); worker != nullptr)
    {
        return worker->getTaskRunning();
    }
    else
    {
        return 0;
    }
}

void NodeController::submitCommand(ConstCommandShrdPtrVec commands)
{
    // submitted command metric
    if (commands.size())
    {
        metrics.incrementCounter(INodeControllerMetricCounterType::SubmittedCommand, commands.size(), submitted_command_labels);
    }
    // apply all submitted commands
    for (auto& cmd : commands)
    {
        // submit command to appropiate worker
        if (auto worker = worker_resolver.resolve(cmd->type); worker != nullptr)
        {
            logger->logMessage(STRING_FORMAT("Forward command => %1% to worker %2%", to_json_string(cmd) % std::string(command_type_to_string(cmd->type))));
            processing_pool->detach_task(
                [this, worker, cmd]
                {
                    worker->processCommand(this->processing_pool, cmd);
                });
        }
        else
        {
            logger->logMessage(STRING_FORMAT("No worker found for command type '%1%'", std::string(command_type_to_string(cmd->type))), LogLevel::ERROR);
            metrics.incrementCounter(INodeControllerMetricCounterType::SubmittedCommand, commands.size(), submitted_command_no_workef_found_labels);
        }
    }
}

// clang-format off
void NodeController::handleStatistic(TaskProperties& task_properties)
{
    proc_system_metrics_grabber.refresh();
    // update all the node system metrics
    system_metrics.incrementCounter(INodeControllerSystemMetricType::VmRSSGauge, proc_system_metrics_grabber.vm_rss, {{"node", node_configuration->getNodeName()}});
    system_metrics.incrementCounter(INodeControllerSystemMetricType::VmHWMGauge, proc_system_metrics_grabber.vm_hwm, {{"node", node_configuration->getNodeName()}});
    system_metrics.incrementCounter(INodeControllerSystemMetricType::VmSizeGauge, proc_system_metrics_grabber.vm_size, {{"node", node_configuration->getNodeName()}});
    system_metrics.incrementCounter(INodeControllerSystemMetricType::VmDataGauge, proc_system_metrics_grabber.vm_data, {{"node", node_configuration->getNodeName()}});
    system_metrics.incrementCounter(INodeControllerSystemMetricType::VmSwapGauge, proc_system_metrics_grabber.vm_swap, {{"node", node_configuration->getNodeName()}});
    system_metrics.incrementCounter(INodeControllerSystemMetricType::RssAnonGauge, proc_system_metrics_grabber.rss_anon, {{"node", node_configuration->getNodeName()}});
    system_metrics.incrementCounter(INodeControllerSystemMetricType::RssFileGauge, proc_system_metrics_grabber.rss_file, {{"node", node_configuration->getNodeName()}});
    system_metrics.incrementCounter(INodeControllerSystemMetricType::ThreadsGauge, proc_system_metrics_grabber.threads, {{"node", node_configuration->getNodeName()}});
    system_metrics.incrementCounter(INodeControllerSystemMetricType::OpenFDsGauge, proc_system_metrics_grabber.open_fds,{{"node", node_configuration->getNodeName()}});
    system_metrics.incrementCounter(INodeControllerSystemMetricType::MaxFDsGauge, proc_system_metrics_grabber.max_fds, {{"node", node_configuration->getNodeName()}});
    system_metrics.incrementCounter(INodeControllerSystemMetricType::UptimeSecGauge, proc_system_metrics_grabber.uptime_sec,{{"node", node_configuration->getNodeName()}});
    system_metrics.incrementCounter(INodeControllerSystemMetricType::UtimeTicksCounter, proc_system_metrics_grabber.utime_ticks,{{"node", node_configuration->getNodeName()}});
    system_metrics.incrementCounter(INodeControllerSystemMetricType::StimeTicksCounter, proc_system_metrics_grabber.stime_ticks,{{"node", node_configuration->getNodeName()}});
    system_metrics.incrementCounter(INodeControllerSystemMetricType::CpuSecondsCounter, proc_system_metrics_grabber.cpu_seconds,{{"node", node_configuration->getNodeName()}});
    system_metrics.incrementCounter(INodeControllerSystemMetricType::IOReadBytesCounter, proc_system_metrics_grabber.io_read_bytes,{{"node", node_configuration->getNodeName()}});
    system_metrics.incrementCounter(INodeControllerSystemMetricType::IOWriteBytesCounter, proc_system_metrics_grabber.io_write_bytes,{{"node", node_configuration->getNodeName()}});
    system_metrics.incrementCounter(INodeControllerSystemMetricType::VoluntaryCtxtSwitchesCounter,proc_system_metrics_grabber.voluntary_ctxt_switches, {{"node", node_configuration->getNodeName()}});
    system_metrics.incrementCounter(INodeControllerSystemMetricType::NonvoluntaryCtxtSwitchesCounter,proc_system_metrics_grabber.nonvoluntary_ctxt_switches, {{"node", node_configuration->getNodeName()}});
    system_metrics.incrementCounter(INodeControllerSystemMetricType::StarttimeJiffiesGauge, proc_system_metrics_grabber.starttime_jiffies,{{"node", node_configuration->getNodeName()}});
}

// clang-format on

void NodeController::startAsGateway()
{
    node_configuration->loadNodeConfiguration();

    // register worker for command type
    // Each command type gets its own worker instance as needed
    logger->logMessage("Configure command executor", LogLevel::INFO);
    auto monitor_command_worker = std::make_shared<MonitorCommandWorker>(this->node_controller_configuration->monitor_command_configuration, epics_service_manager_shrd_ptr, node_configuration);
    if (!monitor_command_worker)
    {
        throw std::runtime_error("Failed to create SnapshotCommandWorker");
    }
    worker_resolver.registerObjectInstance(CommandType::monitor, monitor_command_worker);
    worker_resolver.registerObjectInstance(CommandType::multi_monitor, monitor_command_worker);

    worker_resolver.registerObjectInstance(CommandType::get, std::make_shared<GetCommandWorker>(epics_service_manager_shrd_ptr));
    worker_resolver.registerObjectInstance(CommandType::put, std::make_shared<PutCommandWorker>(epics_service_manager_shrd_ptr));

    auto snapshotCommandWorker = std::make_shared<SnapshotCommandWorker>(this->node_controller_configuration->snapshot_command_configuration, epics_service_manager_shrd_ptr);
    if (!snapshotCommandWorker)
    {
        throw std::runtime_error("Failed to create SnapshotCommandWorker");
    }
    worker_resolver.registerObjectInstance(CommandType::snapshot, snapshotCommandWorker);
    worker_resolver.registerObjectInstance(CommandType::repeating_snapshot, snapshotCommandWorker);
    worker_resolver.registerObjectInstance(CommandType::repeating_snapshot_trigger, snapshotCommandWorker);
    worker_resolver.registerObjectInstance(CommandType::repeating_snapshot_stop, snapshotCommandWorker);
}

void NodeController::performGatewayPeriodicTask()
{
    // Only the monitor worker supports periodic management tasks.
    auto monitor_worker = worker_resolver.resolve(CommandType::monitor);
    dynamic_cast<MonitorCommandWorker*>(monitor_worker.get())->executePeriodicTask();
}

void NodeController::startAsStorage()
{
}

void NodeController::performStoragePeriodicTask()
{
}
