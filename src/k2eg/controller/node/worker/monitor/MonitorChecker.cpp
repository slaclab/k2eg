#include <k2eg/common/utility.h>
#include <k2eg/controller/node/worker/monitor/MonitorChecker.h>
#include <k2eg/service/ServiceResolver.h>

#include <cstddef>
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

MonitorChecker::MonitorChecker(const MonitorCheckerConfiguration& monitor_checker_configuration, configuration::NodeConfigurationShrdPtr node_configuration_db)
    : monitor_checker_configuration(monitor_checker_configuration),
      publisher(ServiceResolver<IPublisher>::resolve()),
      logger(ServiceResolver<ILogger>::resolve()),
      node_configuration_db(node_configuration_db),
      expiration_timeout(monitor_checker_configuration.monitor_expiration_timeout) {}

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

size_t
MonitorChecker::scanForRestart(size_t element_to_process) {
  logger->logMessage("[ Restart Scan ] Start scanning for monitor restart");
  // scan al monitor to check what need to be
  return node_configuration_db->iterateAllChannelMonitorForPurge(
      element_to_process,  // number of unprocessed element to check
      [this](const ChannelMonitorType& monitor_info, int& purge_ts_set_flag) {
        logger->logMessage(STRING_FORMAT("[ Restart Scan - %1% - %2% ] reactivate monitor", monitor_info.pv_name % monitor_info.channel_destination));
        handler_broadcaster.broadcast(MonitorHandlerData{MonitorHandlerAction::Start, monitor_info});
      });
}

#define PURGE_SCAN_LOG_HEADER "Purge Scan"
size_t
MonitorChecker::scanForMonitorToStop(size_t element_to_process) {
  logger->logMessage("[ Purge Scan ] Start scanning for monitor eviction");
  // scan al monitor to check what need to be
  return node_configuration_db->iterateAllChannelMonitorForPurge(
      element_to_process,  // number of unprocessed element to check
      [this](const ChannelMonitorType& monitor_info, int& purge_ts_set_flag) {
        std::lock_guard guard(op_mux);
        logger->logMessage(STRING_FORMAT("[ %1% - %2% - %3% ] check metadata", PURGE_SCAN_LOG_HEADER%monitor_info.pv_name % monitor_info.channel_destination));

        auto queue_metadata = publisher->getQueueMetadata(monitor_info.channel_destination);
        if (queue_metadata && queue_metadata->subscriber_groups.size()) {
          logger->logMessage(STRING_FORMAT("[ %1% - %2% - %3% ] subscriber found %4%",
                                           PURGE_SCAN_LOG_HEADER%monitor_info.pv_name % monitor_info.channel_destination % queue_metadata->subscriber_groups.size()));

          purge_ts_set_flag = -1;
        } else {
          if (monitor_info.start_purge_ts == -1) {
            // set the purge timestamp
            purge_ts_set_flag = 1;
            logger->logMessage(STRING_FORMAT("[ %1% - %2% - %3% ] no subscriber found, set timestamp for purege timeout",
                                             PURGE_SCAN_LOG_HEADER%monitor_info.pv_name % monitor_info.channel_destination));
          } else {
            // we have to check if the timeout is expired
            logger->logMessage(STRING_FORMAT("[ %1% - %2% - %3% ] Check if we need to delete the monitor queue",
                                             PURGE_SCAN_LOG_HEADER%monitor_info.pv_name % monitor_info.channel_destination),
                               LogLevel::ERROR);
            if (isTimeoutExperid(monitor_info)) {
              try {
                logger->logMessage(STRING_FORMAT("[ %1% - %2% - %3% ] Stop monitor", PURGE_SCAN_LOG_HEADER%monitor_info.pv_name % monitor_info.channel_destination),
                                   LogLevel::ERROR);
                handler_broadcaster.broadcast(MonitorHandlerData{MonitorHandlerAction::Stop, monitor_info});

                logger->logMessage(STRING_FORMAT("[ %1% - %2% - %3% ] delete queue", PURGE_SCAN_LOG_HEADER%monitor_info.pv_name % monitor_info.channel_destination),
                                   LogLevel::ERROR);
                publisher->deleteQueue(monitor_info.channel_destination);

                // remove the channel monitor form the database
                node_configuration_db->removeChannelMonitor({monitor_info});
              } catch (...) {
                logger->logMessage(
                    STRING_FORMAT("[ %1% - %2% - %3% ] Error during the stop of the monitor", PURGE_SCAN_LOG_HEADER%monitor_info.pv_name % monitor_info.channel_destination),
                    LogLevel::ERROR);
              }
            } else {
              logger->logMessage(
                  STRING_FORMAT("[ %1% - %2% - %3% ] waiting for timeout exceed", PURGE_SCAN_LOG_HEADER%monitor_info.pv_name % monitor_info.channel_destination));
            }
          }
        }
      });
}

bool
MonitorChecker::isTimeoutExperid(const ChannelMonitorType& monitor_info) {
  if (monitor_info.start_purge_ts == -1) return false;

  auto now_sec_from_epoch = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  return now_sec_from_epoch - monitor_info.start_purge_ts > expiration_timeout;
}

void
MonitorChecker::setPurgeTimeout(int64_t expiration_timeout) {
  if (expiration_timeout < 0) {
    expiration_timeout = monitor_checker_configuration.monitor_expiration_timeout;
  } else {
    this->expiration_timeout = expiration_timeout;
  }
}

void
MonitorChecker::resetMonitorToProcess() {
  node_configuration_db->resetAllChannelMonitorCheck();
}