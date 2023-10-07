#include <k2eg/common/utility.h>
#include <k2eg/controller/node/worker/monitor/MonitorChecker.h>
#include <k2eg/service/ServiceResolver.h>

#include <ostream>

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
    : publisher(ServiceResolver<IPublisher>::resolve()), logger(ServiceResolver<ILogger>::resolve()), node_configuration_db(node_configuration_db) {}

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
    if (result[idx]) { handler_broadcaster.broadcast(MonitorHandlerData{MonitorHandlerAction::Start, channel_descriptions[idx]}); }
  }
}

void
MonitorChecker::scanForMonitorToStop(bool reset_from_beginning) {
  logger->logMessage("[ purge scan ] Start scanning for monitor eviction");
  // scan al monitor to check what need to be
  node_configuration_db->iterateAllChannelMonitor(
      false,
      10,  // number of unprocessed element to check
      [this](const ChannelMonitorType& monitor_info, int& purge_ts_set_flag) {
        std::lock_guard guard(op_mux);
        logger->logMessage(STRING_FORMAT("[  purge scan - %1%-%2% ] get metadata", monitor_info.pv_name % monitor_info.channel_destination));

        auto queue_metadata = publisher->getQueueMetadata(monitor_info.channel_destination);
        if (queue_metadata->subscriber_groups.size()) {
          logger->logMessage(STRING_FORMAT("[  purge scan - %1%-%2% ] subscriber found %3%",
                                           monitor_info.pv_name % monitor_info.channel_destination % queue_metadata->subscriber_groups.size()));
          
          purge_ts_set_flag = -1;
        } else {
          logger->logMessage(STRING_FORMAT("[  purge scan - %1%-%2% ] no subscriber found, set timestamp for purege timeout",
                                           monitor_info.pv_name % monitor_info.channel_destination));
          if(monitor_info.start_purge_ts == -1) {
            // wenever set the pruge timestamp so we need to set it
            purge_ts_set_flag = 1;
            // fire to stop the monitor
            try{
              logger->logMessage(STRING_FORMAT("[  purge scan - %1%-%2% ] Stop monitor",
                                           monitor_info.pv_name % monitor_info.channel_destination), LogLevel::ERROR);
              handler_broadcaster.broadcast(MonitorHandlerData{MonitorHandlerAction::Stop, monitor_info});
              // remove the channel monitor form the database
              node_configuration_db->removeChannelMonitor({monitor_info});
            } catch(...) {
              logger->logMessage(STRING_FORMAT("[  purge scan - %1%-%2% ] Error during the stop of the monitor",
                                           monitor_info.pv_name % monitor_info.channel_destination), LogLevel::ERROR);
            }
            return;
          } 

          // whe have to check if the timeout isexceeded to fire the deletation of the queue
          logger->logMessage(STRING_FORMAT("[  purge scan - %1%-%2% ] Check if we need to delete the monitor queue",
                                           monitor_info.pv_name % monitor_info.channel_destination), LogLevel::ERROR);
          //TODO delete the topic
        }
      });
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