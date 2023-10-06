#include <k2eg/controller/node/worker/monitor/MonitorChecker.h>

using namespace k2eg::controller::node::worker::monitor;
using namespace k2eg::common;
using namespace k2eg::service::data;
using namespace k2eg::service::data::repository;
using namespace k2eg::controller::node::configuration;

MonitorChecker::MonitorChecker(service::pubsub::IPublisherShrdPtr publisher, configuration::NodeConfigurationShrdPtr node_configuration_db)
    : publisher(publisher), node_configuration_db(node_configuration_db) {}

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

// if (start_monitor_command->activate) {
//             // start monitoring
//
//         } else {
//             // stop monitoring
//             node_configuration->removeChannelMonitor(
//                 {ChannelMonitorType{.pv_name = acquire_command_shrd->pv_name,
//                                     .channel_destination = acquire_command_shrd->monitor_destination_topic}});
//         }