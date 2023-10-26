#ifndef k2eg_CONTROLLER_NODE_NODECONTROLLER_H_
#define k2eg_CONTROLLER_NODE_NODECONTROLLER_H_

#include <k2eg/common/ObjectFactory.h>
#include <k2eg/common/types.h>
#include <k2eg/controller/command/CMDCommand.h>
#include <k2eg/controller/node/configuration/NodeConfiguration.h>
#include <k2eg/controller/node/worker/CommandWorker.h>
#include <k2eg/controller/node/worker/monitor/MonitorCommandWorker.h>
#include <k2eg/service/epics/EpicsServiceManager.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/metric/IMetricService.h>

#include <k2eg/common/BS_thread_pool.hpp>
#include <memory>

#include "k2eg/service/data/DataStorage.h"
#include "k2eg/service/metric/INodeControllerMetric.h"

namespace k2eg::controller::node {

struct NodeControllerConfiguration {
  //monitor configuration
  worker::monitor::MonitorCommandConfiguration monitor_command_configuration;

};
DEFINE_PTR_TYPES(NodeControllerConfiguration)
/**
 * Main controller class for the node operation
 */
class NodeController {
  ConstNodeControllerConfigurationUPtr node_controller_configuration;
  std::shared_ptr<BS::thread_pool>                                                                      processing_pool;
  k2eg::common::ObjectByTypeFactory<k2eg::controller::command::cmd::CommandType, worker::CommandWorker> worker_resolver;
  configuration::NodeConfigurationShrdPtr                                                                node_configuration;

  k2eg::service::log::ILoggerShrdPtr             logger;
  k2eg::service::epics_impl::EpicsServiceManager epics_service_manager;
  k2eg::service::metric::INodeControllerMetric&  metric;

 public:
  NodeController(ConstNodeControllerConfigurationUPtr node_controller_configuration, k2eg::service::data::DataStorageShrdPtr data_storage);
  NodeController()                                 = delete;
  NodeController(const NodeController&)            = delete;
  NodeController& operator=(const NodeController&) = delete;
  ~NodeController();
  void performManagementTask();
  void waitForTaskCompletion();
  bool isWorkerReady(k2eg::controller::command::cmd::CommandType cmd_type);
  // Process an array of command
  void submitCommand(k2eg::controller::command::cmd::ConstCommandShrdPtrVec commands);
};
DEFINE_PTR_TYPES(NodeController)
}  // namespace k2eg::controller::node

#endif  // k2eg_CONTROLLER_NODE_NODECONTROLLER_H_