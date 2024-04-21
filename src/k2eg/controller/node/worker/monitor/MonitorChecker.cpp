#include <k2eg/common/utility.h>
#include <k2eg/controller/node/worker/monitor/MonitorChecker.h>
#include <k2eg/service/ServiceResolver.h>

#include <cstddef>
#include <stdexcept>

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
      expiration_timeout(monitor_checker_configuration.monitor_expiration_timeout) {
  for (auto& regex : monitor_checker_configuration.filter_out_regex) { vec_fillout_regex.push_back(std::regex(regex)); }
}

MonitorChecker::~MonitorChecker() {}

BroadcastToken
MonitorChecker::addHandler(CheckerEventHandler handler) {
  std::lock_guard guard(op_mux);
  return handler_broadcaster.registerHandler(handler);
}

void
MonitorChecker::storeMonitorData(const ChannelMonitorTypeConstVector& channel_descriptions) {
  {
    std::lock_guard guard(op_mux);
    node_configuration_db->addChannelMonitor(channel_descriptions);
  }
  for (int idx = 0; idx < channel_descriptions.size(); idx++) { handler_broadcaster.broadcast(MonitorHandlerData{MonitorHandlerAction::Start, channel_descriptions[idx]}); }
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

bool
MonitorChecker::excludeConsumer(std::string consumer_group_name) {
  bool exclude = false;
  for (auto& r : vec_fillout_regex) {
    exclude = std::regex_match(consumer_group_name, r);
    if (exclude) break;
  }
  return exclude;
}

#define PURGE_SCAN_LOG_HEADER "Purge Scan"
size_t
MonitorChecker::scanForMonitorToStop(size_t element_to_process) {
  logger->logMessage("[ Purge Scan ] Start scanning for monitor eviction");
  // scan al monitor to check what need to be
  //TODO this raise memory leaks
  return node_configuration_db->iterateAllChannelMonitorForPurge(
      element_to_process,  // number of unprocessed element to check
      [this](const ChannelMonitorType& monitor_info, int& purge_ts_set_flag) {
        try{
          std::lock_guard guard(op_mux);
          int  filtered_out   = 0;
          logger->logMessage(STRING_FORMAT("[ %1% - %2% - %3% ] check metadata", PURGE_SCAN_LOG_HEADER % monitor_info.pv_name % monitor_info.channel_destination));
          auto queue_metadata = publisher->getQueueMetadata(monitor_info.channel_destination);
          // take in consideration only the consuemr that are not filter ot from the regex
          for (auto& cg : queue_metadata->subscriber_groups) {
            if (excludeConsumer(cg->name)) {
              filtered_out++;
            } else if (!cg->subscribers.size()) {
              // we filter out also if the consumer gorup haven't any consumer
              filtered_out++;
            }
          }
          if (queue_metadata && (queue_metadata->subscriber_groups.size() - filtered_out)) {
            logger->logMessage(STRING_FORMAT(
                "[ %1% - %2% - %3% ] subscriber found %4%",
                PURGE_SCAN_LOG_HEADER % monitor_info.pv_name % monitor_info.channel_destination % (queue_metadata->subscriber_groups.size() - filtered_out)));

            purge_ts_set_flag = -1;
          } else {
            if (monitor_info.start_purge_ts == -1) {
              // set the purge timestamp
              purge_ts_set_flag = 1;
              logger->logMessage(STRING_FORMAT("[ %1% - %2% - %3% ] no subscriber found, set timestamp for purege timeout",
                                              PURGE_SCAN_LOG_HEADER % monitor_info.pv_name % monitor_info.channel_destination));
            } else {
              // we have to check if the timeout is expired
              logger->logMessage(STRING_FORMAT("[ %1% - %2% - %3% ] Check if we need to delete the monitor queue",
                                              PURGE_SCAN_LOG_HEADER % monitor_info.pv_name % monitor_info.channel_destination));
              if (isTimeoutExperid(monitor_info)) {
                try {
                  logger->logMessage(
                      STRING_FORMAT("[ %1% - %2% - %3% ] Stop monitor", PURGE_SCAN_LOG_HEADER % monitor_info.pv_name % monitor_info.channel_destination));
                  handler_broadcaster.broadcast(MonitorHandlerData{MonitorHandlerAction::Stop, monitor_info});

                  // check if we need to delete the also the topic of the monitor
                  if (monitor_checker_configuration.purge_queue_on_monitor_timeout) {
                    logger->logMessage(
                        STRING_FORMAT("[ %1% - %2% - %3% ] delete queue", PURGE_SCAN_LOG_HEADER % monitor_info.pv_name % monitor_info.channel_destination));
                    publisher->deleteQueue(monitor_info.channel_destination);
                  }

                  // remove the channel monitor form the database
                  node_configuration_db->removeChannelMonitor({monitor_info});
                } catch (...) {
                  logger->logMessage(STRING_FORMAT("[ %1% - %2% - %3% ] Error during the stop of the monitor",
                                                  PURGE_SCAN_LOG_HEADER % monitor_info.pv_name % monitor_info.channel_destination),
                                    LogLevel::ERROR);
                }
              } else {
                logger->logMessage(STRING_FORMAT("[ %1% - %2% - %3% ] waiting for timeout exceed",
                                                PURGE_SCAN_LOG_HEADER % monitor_info.pv_name % monitor_info.channel_destination));
              }
            }
          }
        } catch (std::runtime_error& e) {
          logger->logMessage(STRING_FORMAT("[ %1% - %2% - %3% ] Error during the stop of the monitor", PURGE_SCAN_LOG_HEADER % monitor_info.pv_name % monitor_info.channel_destination),
                             LogLevel::ERROR);
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