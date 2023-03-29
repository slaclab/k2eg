#ifndef k2eg_CONTROLLER_NODE_NODECONTROLLER_H_
#define k2eg_CONTROLLER_NODE_NODECONTROLLER_H_


#include <k2eg/common/types.h>

#include <k2eg/common/BS_thread_pool.hpp>
#include <k2eg/controller/command/CMDCommand.h>
#include <k2eg/controller/node/configuration/NodeConfiguration.h>
#include <k2eg/controller/node/worker/CommandWorker.h>
#include <k2eg/controller/node/worker/WorkerResolver.h>

#include <k2eg/service/log/ILogger.h>

#include<k2eg/service/epics/EpicsServiceManager.h>

#include <memory>

namespace k2eg::controller::node {
/**
 * Main controller class for the node operation
 */
class NodeController {
    std::shared_ptr<BS::thread_pool> processing_pool;
    worker::WorkerResolver<worker::CommandWorker> worker_resolver;
    configuration::NodeConfigurationUPtr node_configuration;

    k2eg::service::log::ILoggerShrdPtr logger;
    k2eg::service::epics_impl::EpicsServiceManager epics_service_manager;
public:
    NodeController(k2eg::service::data::DataStorageUPtr data_storage);
    NodeController() = delete;
    NodeController(const NodeController&) = delete;
    NodeController& operator=(const NodeController&) = delete;
    ~NodeController();

    // apply all the spermanent command
    void reloadPersistentCommand();
    
    // Process an array of command
    void submitCommand(k2eg::controller::command::CommandConstShrdPtrVec commands);
};
DEFINE_PTR_TYPES(NodeController)
} // namespace k2eg::controller::node

#endif // k2eg_CONTROLLER_NODE_NODECONTROLLER_H_