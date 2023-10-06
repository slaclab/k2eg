#include <k2eg/common/utility.h>
#include <k2eg/controller/node/worker/monitor/MonitorCommandWorker.h>
#include <k2eg/service/ServiceResolver.h>

#include <cassert>
#include <functional>

#include "k2eg/controller/command/cmd/MonitorCommand.h"
#include "k2eg/controller/node/worker/CommandWorker.h"
#include "k2eg/controller/node/worker/monitor/MonitorChecker.h"
#include "k2eg/service/epics/EpicsChannel.h"
#include "k2eg/service/log/ILogger.h"
#include "k2eg/service/metric/IMetricService.h"
#include "k2eg/service/pubsub/IPublisher.h"

using namespace k2eg::common;

using namespace k2eg::controller::node::configuration;
using namespace k2eg::controller::node::worker;
using namespace k2eg::controller::node::worker::monitor;
using namespace k2eg::controller::command;
using namespace k2eg::controller::command::cmd;

using namespace k2eg::service;
using namespace k2eg::service::log;
using namespace k2eg::service::epics_impl;
using namespace k2eg::service::pubsub;
using namespace k2eg::service::metric;
using namespace k2eg::service::data;
using namespace k2eg::service::data::repository;

#pragma region MonitorCommandWorker
MonitorCommandWorker::MonitorCommandWorker(EpicsServiceManagerShrdPtr epics_service_manager, NodeConfigurationShrdPtr node_configuration_db)
    : node_configuration_db(node_configuration_db),
      logger(ServiceResolver<ILogger>::resolve()),
      publisher(ServiceResolver<IPublisher>::resolve()),
      metric(ServiceResolver<IMetricService>::resolve()->getEpicsMetric()),
      epics_service_manager(epics_service_manager),
      monitor_checker_shrd_ptr(MakeMonitorCheckerShrdPtr(node_configuration_db)) {
  handler_token = epics_service_manager->addHandler(std::bind(&MonitorCommandWorker::epicsMonitorEvent, this, std::placeholders::_1));
}

void
MonitorCommandWorker::manageStartMonitorCommand(ConstMonitorCommandShrdPtr cmd_ptr) {
  // ensure database is update with monitor information
  node_configuration_db->addChannelMonitor({ChannelMonitorType{.pv_name             = cmd_ptr->pv_name,
                                                               .event_serialization = static_cast<std::uint8_t>(cmd_ptr->serialization),
                                                               .channel_protocol    = cmd_ptr->protocol,
                                                               .channel_destination = cmd_ptr->monitor_destination_topic}});
  // lock the map and contained vector for write
  {
    if (cmd_ptr->monitor_destination_topic.empty()) {
      logger->logMessage(STRING_FORMAT("No destination topic found on monitor command for %1%", cmd_ptr->pv_name), LogLevel::ERROR);
      return;
    }

    std::unique_lock lock(channel_map_mtx);
    logger->logMessage(STRING_FORMAT("%1% monitor on '%2%' for topic '%3%'",
                                     (cmd_ptr->activate ? "Activate" : "Deactivate") % cmd_ptr->pv_name % cmd_ptr->monitor_destination_topic));

    auto& vec_ref = channel_topics_map[cmd_ptr->pv_name];
    if (cmd_ptr->activate) {
      // add topic to channel
      // check if the topic is already present[fault tollerant check]
      if (std::find_if(std::begin(vec_ref), std::end(vec_ref), [&cmd_ptr](auto& info_topic) {
            return info_topic->cmd->monitor_destination_topic.compare(cmd_ptr->monitor_destination_topic) == 0;
          }) == std::end(vec_ref)) {
        channel_topics_map[cmd_ptr->pv_name].push_back(MakeChannelTopicMonitorInfoUPtr(ChannelTopicMonitorInfo{cmd_ptr}));
      } else {
        logger->logMessage(STRING_FORMAT("Monitor for '%1%' for topic '%2%' already activated", cmd_ptr->pv_name % cmd_ptr->monitor_destination_topic));
      }
    } else {
      // remove topic to channel
      auto itr = std::find_if(std::begin(vec_ref), std::end(vec_ref), [&cmd_ptr](auto& info_topic) {
        return info_topic->cmd->monitor_destination_topic.compare(cmd_ptr->monitor_destination_topic) == 0;
      });
      if (itr != std::end(vec_ref)) {
        vec_ref.erase(itr);
      } else {
        logger->logMessage(STRING_FORMAT("No active monitor on '%1%' for topic '%2%'", cmd_ptr->pv_name % cmd_ptr->monitor_destination_topic));
      }
    }
  }
  // if the vec_ref has size > 0 mean that someone is still needed the channel data in monitor way
  epics_service_manager->monitorChannel(cmd_ptr->pv_name, true, cmd_ptr->protocol);
  // send reply
  manageReply(0, "Monitor activated", cmd_ptr);
}

void
MonitorCommandWorker::processCommand(ConstCommandShrdPtr command) {
  if (command->type != CommandType::monitor) return;
  bool activate = false;
  auto cmd_ptr  = static_pointer_cast<const MonitorCommand>(command);
  if (cmd_ptr->activate) {
    manageStartMonitorCommand(cmd_ptr);
  } else {
    logger->logMessage(STRING_FORMAT("Deactivation for monitor is deprecated[%1%-%2%]", cmd_ptr->pv_name % cmd_ptr->monitor_destination_topic),
                       LogLevel::ERROR);
  }
}

void
MonitorCommandWorker::manageReply(const std::int8_t error_code, const std::string& error_message, ConstMonitorCommandShrdPtr cmd) {
  logger->logMessage(STRING_FORMAT("%1% [pv:%2%]", error_message % cmd->pv_name), LogLevel::ERROR);
  if (cmd->reply_topic.empty() || cmd->reply_id.empty()) {
    return;
  } else {
    auto serialized_message = serialize(MonitorCommandReply{error_code, cmd->reply_id, error_message}, cmd->serialization);
    if (!serialized_message) {
      logger->logMessage("Invalid serialized message", LogLevel::FATAL);
    } else {
      publisher->pushMessage(MakeReplyPushableMessageUPtr(cmd->reply_topic, "monitor-operation", cmd->pv_name, serialized_message),
                             {{"k2eg-ser-type", serialization_to_string(cmd->serialization)}});
    }
  }
}

void
MonitorCommandWorker::epicsMonitorEvent(EpicsServiceManagerHandlerParamterType event_received) {
#ifdef __DEBUG__
  logger->logMessage(STRING_FORMAT("Received epics monitor %1% events data", event_received->event_data->size()), LogLevel::TRACE);
#endif
  //----------update metric--------
  metric.incrementCounter(IEpicsMetricCounterType::MonitorData, event_received->event_data->size());
  metric.incrementCounter(IEpicsMetricCounterType::MonitorCancel, event_received->event_cancel->size());
  metric.incrementCounter(IEpicsMetricCounterType::MonitorDisconnect, event_received->event_disconnect->size());
  metric.incrementCounter(IEpicsMetricCounterType::MonitorFail, event_received->event_fail->size());

  std::shared_lock slock(channel_map_mtx);
  // cache the various serilized message for each serializaiton type
  std::map<SerializationType, ConstSerializedMessageShrdPtr> local_serialization_cache;
  for (auto& event : *event_received->event_data) {
    // publisher
    for (auto& info_topic : channel_topics_map[event->channel_data.pv_name]) {
      logger->logMessage(STRING_FORMAT("Publish channel %1% on topic %2%", event->channel_data.pv_name % info_topic->cmd->monitor_destination_topic),
                         LogLevel::TRACE);
      if (!local_serialization_cache.contains(info_topic->cmd->serialization)) {
        // cache new serialized message
        local_serialization_cache[info_topic->cmd->serialization] =
            serialize(event->channel_data, static_cast<SerializationType>(info_topic->cmd->serialization));
      }
      publisher->pushMessage(MakeReplyPushableMessageUPtr(info_topic->cmd->monitor_destination_topic,
                                                          "monitor-message",
                                                          event->channel_data.pv_name,
                                                          local_serialization_cache[info_topic->cmd->serialization]),
                             {// add header
                              {"k2eg-ser-type", serialization_to_string(info_topic->cmd->serialization)}});
    }
  }
  publisher->flush(100);
}
#pragma endregion MonitorMessage