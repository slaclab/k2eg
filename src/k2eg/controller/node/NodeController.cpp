#include <k2eg/common/utility.h>
#include <k2eg/common/BS_thread_pool.hpp>

#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/ServiceResolver.h>
#include <k2eg/service/data/DataStorage.h>
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

NodeController::NodeController(ConstNodeControllerConfigurationUPtr node_controller_configuration, DataStorageShrdPtr data_storage)
    : node_controller_configuration(std::move(node_controller_configuration))
    , node_configuration(std::make_shared<NodeConfiguration>(data_storage))
    , processing_pool(std::make_shared<BS::light_thread_pool>())
    , metric(ServiceResolver<IMetricService>::resolve()->getNodeControllerMetric())
{
    // set logger
    logger = ServiceResolver<ILogger>::resolve();

    // load current node configuration
    logger->logMessage("Load node configuration", LogLevel::INFO);
    node_configuration->loadNodeConfiguration();

    // register worker for command type
    logger->logMessage("Configure command executor", LogLevel::INFO);
    auto monitor_command_worker = std::make_shared<MonitorCommandWorker>(this->node_controller_configuration->monitor_command_configuration, ServiceResolver<EpicsServiceManager>::resolve(), node_configuration);
    worker_resolver.registerObjectInstance(CommandType::monitor, monitor_command_worker);
    worker_resolver.registerObjectInstance(CommandType::multi_monitor, monitor_command_worker);
    worker_resolver.registerObjectInstance(CommandType::get, std::make_shared<GetCommandWorker>(ServiceResolver<EpicsServiceManager>::resolve()));
    worker_resolver.registerObjectInstance(CommandType::put, std::make_shared<PutCommandWorker>(ServiceResolver<EpicsServiceManager>::resolve()));
    worker_resolver.registerObjectInstance(CommandType::snapshot, std::make_shared<SnapshotCommandWorker>(ServiceResolver<EpicsServiceManager>::resolve()));
}

NodeController::~NodeController()
{
    processing_pool->wait();
}

void NodeController::performManagementTask()
{
    auto monitor_worker = worker_resolver.resolve(CommandType::monitor);
    dynamic_cast<MonitorCommandWorker*>(monitor_worker.get())->executePeriodicTask();
}

void NodeController::waitForTaskCompletion()
{
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

void NodeController::submitCommand(ConstCommandShrdPtrVec commands)
{
    // submitted command metric
    if (commands.size())
    {
        metric.incrementCounter(INodeControllerMetricCounterType::SubmittedCommand, commands.size());
    }
    // apply all submitted commands
    for (auto& cmd : commands)
    {
        logger->logMessage(STRING_FORMAT("Process command => %1%", to_json_string(cmd)));
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
        }
    }
}