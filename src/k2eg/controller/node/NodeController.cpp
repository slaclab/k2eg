#include <k2eg/controller/node/NodeController.h>

//------------ command include ----------
#include <k2eg/controller/node/worker/AcquireCommandWorker.h>
#include <k2eg/controller/node/worker/GetCommandWorker.h>
#include <k2eg/controller/node/worker/WorkerResolver.h>
#include <k2eg/service/ServiceResolver.h>

#include <k2eg/common/utility.h>

using namespace k2eg::controller::node;
using namespace k2eg::controller::node::worker;
using namespace k2eg::controller::node::configuration;

using namespace k2eg::controller::command;

using namespace k2eg::service;
using namespace k2eg::service::data;
using namespace k2eg::service::data::repository;
using namespace k2eg::service::log;
using namespace k2eg::service::epics_impl;

void executeInThreadPool(CommandWorkerShrdPtr worker, CommandConstShrdPtr cmd) {
    auto logger = ServiceResolver<ILogger>::resolve();
    if(!worker->processCommand(cmd)) {
        logger->logMessage(STRING_FORMAT("Error submitting command to worker for type '%1%'",std::string(command_type_to_string(cmd->type))));
    }
}

NodeController::NodeController(DataStorageUPtr data_storage)
    : node_configuration(std::make_unique<NodeConfiguration>(std::move(data_storage)))
    , processing_pool(std::make_shared<BS::thread_pool>()) {
    // set logger
    logger = ServiceResolver<ILogger>::resolve();

    // register worker for command type
    worker_resolver.registerService(
        CommandType::monitor,
        std::make_shared<AcquireCommandWorker>(ServiceResolver<EpicsServiceManager>::resolve()));
    worker_resolver.registerService(
        CommandType::get,
        std::make_shared<GetCommandWorker>(ServiceResolver<EpicsServiceManager>::resolve()));
}

NodeController::~NodeController() { processing_pool->wait_for_tasks(); }

void NodeController::reloadPersistentCommand() {
    node_configuration->iterateAllChannelMonitor(
        [this](uint32_t index, const ChannelMonitorType& monitor_element) {
           auto command =  fromChannelMonitor(monitor_element);
           submitCommand({command});
        }   
    );
}

void NodeController::submitCommand(CommandConstShrdPtrVec commands) {
    // scann and process al command
    for (auto& c: commands) {
        switch (c->type) {
        case CommandType::monitor: {
            std::shared_ptr<const AquireCommand> acquire_command_shrd = static_pointer_cast<const AquireCommand>(c);
            if (acquire_command_shrd->activate) {
                // start monitoring
                node_configuration->addChannelMonitor(
                    {ChannelMonitorType{.channel_name = acquire_command_shrd->channel_name,
                                        .channel_protocol = acquire_command_shrd->protocol,
                                        .channel_destination = acquire_command_shrd->destination_topic}});
            } else {
                // stop monitoring
                node_configuration->removeChannelMonitor(
                    {ChannelMonitorType{.channel_name = acquire_command_shrd->channel_name,
                                        .channel_destination = acquire_command_shrd->destination_topic}});
            }
            break;
        }
        default:
            break;
        }

        logger->logMessage(STRING_FORMAT("Process command => %1%",to_json_string(c)));
        // submit command to appropiate worker
        if (auto worker = worker_resolver.resolve(c->type); worker != nullptr) {
            auto type_str = command_type_to_string(c->type);
            processing_pool->push_task(executeInThreadPool, worker, c);
        } else {
            logger->logMessage(STRING_FORMAT("No worker found for command type '%1%'",std::string(command_type_to_string(c->type))));
        }
    }
}