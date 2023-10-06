#include <k2eg/controller/node/worker/monitor/MonitorChecker.h>
#include <k2eg/service/ServiceResolver.h>

#include <ostream>
#include <k2eg/common/utility.h>
#include "k2eg/service/log/ILogger.h"

using namespace k2eg::controller::node::worker::monitor;
using namespace k2eg::common;

using namespace k2eg::service;
using namespace k2eg::service::pubsub;
using namespace k2eg::service::log;
using namespace k2eg::service::data;
using namespace k2eg::service::data::repository;
using namespace k2eg::controller::node::configuration;

MonitorChecker::MonitorChecker(configuration::NodeConfigurationShrdPtr node_configuration_db)
    : publisher(ServiceResolver<IPublisher>::resolve())
    , logger(ServiceResolver<ILogger>::resolve())
    , node_configuration_db(node_configuration_db) {}

MonitorChecker::~MonitorChecker() {}

BroadcastToken
MonitorChecker::addHandler(CheckerEventHandler handler) {
  std::lock_guard guard(op_mux);
  return handler_broadcaster.registerHandler(handler);
}

void
MonitorChecker::storeMonitorData(const ChannelMonitorTypeConstVector& channel_descriptions) {
  std::lock_guard guard(op_mux);
  auto            result = node_configuration_db->addChannelMonitor(channel_descriptions);
  if (handler_broadcaster.targets.size() == 0) return;
  for (int idx = 0; idx < result.size(); idx++) {
    if (result[idx]) {
      handler_broadcaster.broadcast(MonitorHandlerData{MonitorHandlerAction::Start, channel_descriptions[idx]});
    }
  }
}


void
MonitorChecker::scanForMonitorToStop(bool reset_from_beginning) {
  logger->logMessage("Start scanning for monitor eviction");
  // scan al monitor to check what need to be
  node_configuration_db->iterateAllChannelMonitor(
    false,
    10,// number of unprocessed element to 
    [this](uint32_t index, const ChannelMonitorType& monitor_info){
      logger->logMessage(STRING_FORMAT("Check for %1%-%2%", monitor_info.pv_name%monitor_info.channel_destination));
    }
  );

}
// if (start_monitor_command->activate) {
//             // start monitoring
//
//         } else {
//             // stop monitoring
//             node_configuration->removeChannelMonitor(
//                 {ChannelMonitorType{.pv_name = acquire_command_shrd->pv_name,
//                                     .channel_destination = acquire_command_shrd->monitor_destination_topic}});
//         }