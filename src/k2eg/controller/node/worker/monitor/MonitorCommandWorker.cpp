#include <k2eg/common/utility.h>
#include <k2eg/controller/node/worker/monitor/MonitorCommandWorker.h>
#include <k2eg/service/ServiceResolver.h>
#include <k2eg/service/scheduler/Scheduler.h>

#include <cassert>
#include <functional>
#include <mutex>

#include "k2eg/controller/command/cmd/Command.h"
#include "k2eg/controller/command/cmd/MonitorCommand.h"
#include "k2eg/controller/node/worker/CommandWorker.h"
#include "k2eg/controller/node/worker/monitor/MonitorChecker.h"
#include "k2eg/service/epics/EpicsChannel.h"
#include "k2eg/service/log/ILogger.h"
#include "k2eg/service/metric/IMetricService.h"
#include "k2eg/service/pubsub/IPublisher.h"
#include "k2eg/service/scheduler/Task.h"

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
using namespace k2eg::service::scheduler;
using namespace k2eg::service::data;
using namespace k2eg::service::data::repository;

#pragma region MonitorCommandWorker
#define MAINTANACE_TASK_NAME           "maintanance-task"
#define STARTUP_MONITOR_TASK_NAME      "startup-task"
#define STARTUP_MONITOR_TASK_NAME_CRON "* * * * * *"

MonitorCommandWorker::MonitorCommandWorker(const MonitorCommandConfiguration& monitor_command_configuration,
                                           EpicsServiceManagerShrdPtr         epics_service_manager,
                                           NodeConfigurationShrdPtr           node_configuration_db)
    : monitor_command_configuration(monitor_command_configuration),
      node_configuration_db(node_configuration_db),
      logger(ServiceResolver<ILogger>::resolve()),
      publisher(ServiceResolver<IPublisher>::resolve()),
      metric(ServiceResolver<IMetricService>::resolve()->getEpicsMetric()),
      epics_service_manager(epics_service_manager),
      monitor_checker_shrd_ptr(MakeMonitorCheckerShrdPtr(monitor_command_configuration.monitor_checker_configuration, node_configuration_db)),
      starting_up(true) {
  // reset all processed element present in the monitor database
  monitor_checker_shrd_ptr->resetMonitorToProcess();
  // add epics monitor handler
  epics_handler_token = epics_service_manager->addHandler(std::bind(&MonitorCommandWorker::epicsMonitorEvent, this, std::placeholders::_1));
  // add monitor checker handler
  monitor_checker_token = monitor_checker_shrd_ptr->addHandler(std::bind(&MonitorCommandWorker::handleMonitorCheckEvents, this, std::placeholders::_1));

  // start checker timing
  auto task_periodic_maintanance = MakeTaskShrdPtr(MAINTANACE_TASK_NAME,
                                                   monitor_command_configuration.cron_scheduler_monitor_check,
                                                   std::bind(&MonitorCommandWorker::handlePeriodicTask, this, std::placeholders::_1));
  ServiceResolver<Scheduler>::resolve()->addTask(task_periodic_maintanance);
  auto task_restart_monitor = MakeTaskShrdPtr(
      STARTUP_MONITOR_TASK_NAME, STARTUP_MONITOR_TASK_NAME_CRON, std::bind(&MonitorCommandWorker::handleRestartMonitorTask, this, std::placeholders::_1));
  ServiceResolver<Scheduler>::resolve()->addTask(task_restart_monitor);
}

MonitorCommandWorker::~MonitorCommandWorker() {
  // dipose all still live monitor
  logger->logMessage("[ Exing Worker ] stop all still live monitor");
  for (auto& mon_vec_for_pv : channel_topics_map) {
    logger->logMessage(STRING_FORMAT("[ Exing Worker ] Stop all monitor for pv '%1%'", mon_vec_for_pv.first));
    for (auto& monitor_info : mon_vec_for_pv.second) {
      logger->logMessage(
          STRING_FORMAT("[ Exing Worker ] Stop monitor for pv '%1%' with target '%2%'", monitor_info->cmd.pv_name % monitor_info->cmd.channel_destination));
      epics_service_manager->monitorChannel(monitor_info->cmd.pv_name, false);
    }
  }

  // dispose the token for the event
  epics_handler_token.reset();
  monitor_checker_token.reset();
  logger->logMessage("Remove periodic task from scheduler", LogLevel::DEBUG);
  bool erased = ServiceResolver<Scheduler>::resolve()->removeTaskByName(MAINTANACE_TASK_NAME);
  logger->logMessage(STRING_FORMAT("Remove periodic maintanance : %1%", erased));

  erased = ServiceResolver<Scheduler>::resolve()->removeTaskByName(STARTUP_MONITOR_TASK_NAME);
  logger->logMessage(STRING_FORMAT("Remove startup task: %1%", erased));
}

void
MonitorCommandWorker::handleRestartMonitorTask(TaskProperties& task_properties) {
  std::lock_guard<std::mutex> lock(periodic_task_mutex);
  logger->logMessage("[ Startup Task ] Restart monitor requests");
  task_properties.completed = !(starting_up = monitor_checker_shrd_ptr->scanForRestart());
  if(!starting_up) {
    logger->logMessage("[ Startup Task ] Startup completed");
  }
}

void
MonitorCommandWorker::handlePeriodicTask(TaskProperties& task_properties) {
  std::lock_guard<std::mutex> lock(periodic_task_mutex);
  logger->logMessage("[ Automatic Task ] Checking active monitor");
  auto processed = monitor_checker_shrd_ptr->scanForMonitorToStop();
  if (!processed){
    monitor_checker_shrd_ptr->resetMonitorToProcess();
    logger->logMessage("[ Automatic Task ] All monitor has been checked");
  }
}

void
MonitorCommandWorker::executePeriodicTask() {
  TaskProperties task_properties;
  handlePeriodicTask(task_properties);
}

void
MonitorCommandWorker::handleMonitorCheckEvents(MonitorHandlerData checker_event_data) {
   auto sanitized_pv = epics_service_manager->sanitizePVName(checker_event_data.monitor_type.pv_name);
  if(!sanitized_pv) {
    logger->logMessage(STRING_FORMAT("Error on sanitization for '%1%'", checker_event_data.monitor_type.pv_name ));
    return;
  }
  auto& vec_ref = channel_topics_map[sanitized_pv->name];
  switch (checker_event_data.action) {
    case MonitorHandlerAction::Start: {
      logger->logMessage(STRING_FORMAT("Activate monitor on '%1%' for topic '%2%'",
                                       checker_event_data.monitor_type.pv_name % checker_event_data.monitor_type.channel_destination));
      // got start event
      if (std::find_if(std::begin(vec_ref), std::end(vec_ref), [&checker_event_data](auto& info_topic) {
            return info_topic->cmd.channel_destination.compare(checker_event_data.monitor_type.channel_destination) == 0;
          }) == std::end(vec_ref)) {
        channel_topics_map[sanitized_pv->name].push_back(
            MakeChannelTopicMonitorInfoUPtr(ChannelTopicMonitorInfo{checker_event_data.monitor_type}));
        epics_service_manager->monitorChannel(checker_event_data.monitor_type.pv_name, true);
      } else {
        logger->logMessage(STRING_FORMAT("Monitor for '%1%' for topic '%2%' already activated",
                                         checker_event_data.monitor_type.pv_name % checker_event_data.monitor_type.channel_destination));
      }
      break;
    }

    case MonitorHandlerAction::Stop: {
      // got stop event
      // remove topic to channel
      logger->logMessage(STRING_FORMAT("Stop monitor on '%1%' for topic '%2%'",
                                       checker_event_data.monitor_type.pv_name % checker_event_data.monitor_type.channel_destination));
      auto itr = std::find_if(std::begin(vec_ref), std::end(vec_ref), [&checker_event_data](auto& info_topic) {
        return info_topic->cmd.channel_destination.compare(checker_event_data.monitor_type.channel_destination) == 0;
      });
      if (itr != std::end(vec_ref)) {
        vec_ref.erase(itr);
        epics_service_manager->monitorChannel(checker_event_data.monitor_type.pv_name, false);
      } else {
        logger->logMessage(STRING_FORMAT("No active monitor on '%1%' for topic '%2%'",
                                         checker_event_data.monitor_type.pv_name % checker_event_data.monitor_type.channel_destination));
      }
      break;
    }
  }
}

void
MonitorCommandWorker::processCommand(ConstCommandShrdPtr command) {
  if (starting_up) {
    logger->logMessage("[ Starting up ] Comamnd cannot be executed");
    manageReply(-2, "Command cannot be executed, k2eg monitor worker is starting", command);
    return;
  }
  switch (command->type) {
    case CommandType::monitor: manage_single_monitor(command); break;
    case CommandType::multi_monitor: manage_multiple_monitor(command); break;
    default: break;
  }
}

void
MonitorCommandWorker::manage_single_monitor(k2eg::controller::command::cmd::ConstCommandShrdPtr command) {
  bool activate = false;
  auto cmd_ptr  = static_pointer_cast<const MonitorCommand>(command);
  if (cmd_ptr->activate) {
    if (cmd_ptr->monitor_destination_topic.empty()) {
      logger->logMessage(STRING_FORMAT("No destination topic found on monitor command for %1%", cmd_ptr->pv_name), LogLevel::ERROR);
      manageReply(-1, "Empty destination topic", cmd_ptr);
      return;
    }
    // manageStartMonitorCommand(cmd_ptr);
    monitor_checker_shrd_ptr->storeMonitorData({ChannelMonitorType{.pv_name             = cmd_ptr->pv_name,
                                                                   .event_serialization = static_cast<std::uint8_t>(cmd_ptr->serialization),
                                                                  //  .channel_protocol    = cmd_ptr->protocol,
                                                                   .channel_destination = cmd_ptr->monitor_destination_topic}});
    manageReply(0, STRING_FORMAT("Monitor activated for %1%", cmd_ptr->pv_name), cmd_ptr);
  } else {
    const std::string error_message = STRING_FORMAT("Deactivation for monitor is deprecated[%1%-%2%]", cmd_ptr->pv_name % cmd_ptr->monitor_destination_topic);
    logger->logMessage(error_message, LogLevel::ERROR);
    manageReply(-1, error_message, cmd_ptr);
  }
}

const std::string
MonitorCommandWorker::get_queue_for_pv(const std::string& pv_name) {
  return std::regex_replace(pv_name, std::regex(":"), "_");
}

void
MonitorCommandWorker::manage_multiple_monitor(k2eg::controller::command::cmd::ConstCommandShrdPtr command) {
  bool                            activate = false;
  auto                            cmd_ptr  = static_pointer_cast<const MultiMonitorCommand>(command);
  std::vector<ChannelMonitorType> monitor_command_vec;
  std::ranges::for_each(cmd_ptr->pv_name_list, [&cmd_ptr, &monitor_command_vec, this](const std::string& pv_name) {
    //extract all pv component
    auto sanitized_pv = epics_service_manager->sanitizePVName(pv_name);
    if(!sanitized_pv) {
      logger->logMessage(STRING_FORMAT("Error on sanitization for '%1%'", pv_name));
      return;
    }
    monitor_command_vec.push_back(ChannelMonitorType{.pv_name             = pv_name,
                                                     .event_serialization = static_cast<std::uint8_t>(cmd_ptr->serialization),
                                                    //  .channel_protocol    = cmd_ptr->protocol,
                                                     .channel_destination = get_queue_for_pv(sanitized_pv->name)});
  });
  monitor_checker_shrd_ptr->storeMonitorData(monitor_command_vec);
  manageReply(0, "Monitor activated", cmd_ptr);
}

bool
MonitorCommandWorker::isReady() {
  return !starting_up;
}

void
MonitorCommandWorker::manageReply(const std::int8_t error_code, const std::string& error_message, ConstCommandShrdPtr cmd) {
  logger->logMessage(error_message, LogLevel::ERROR);
  if (cmd->reply_topic.empty() || cmd->reply_id.empty()) {
    return;
  } else {
    auto serialized_message = serialize(MonitorCommandReply{error_code, cmd->reply_id, error_message}, cmd->serialization);
    if (!serialized_message) {
      logger->logMessage("Invalid serialized message", LogLevel::FATAL);
    } else {
      publisher->pushMessage(MakeReplyPushableMessageUPtr(cmd->reply_topic, "monitor-operation", "monitor-error-key", serialized_message),
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
      logger->logMessage(STRING_FORMAT("Publish channel %1% on topic %2%", event->channel_data.pv_name % info_topic->cmd.channel_destination), LogLevel::TRACE);
      if (!local_serialization_cache.contains(static_cast<SerializationType>(info_topic->cmd.event_serialization))) {
        // cache new serialized message
        local_serialization_cache[static_cast<SerializationType>(info_topic->cmd.event_serialization)] =
            serialize(event->channel_data, static_cast<SerializationType>(info_topic->cmd.event_serialization));
      }
      publisher->pushMessage(MakeReplyPushableMessageUPtr(info_topic->cmd.channel_destination,
                                                          "monitor-message",
                                                          event->channel_data.pv_name,
                                                          local_serialization_cache[static_cast<SerializationType>(info_topic->cmd.event_serialization)]),
                             {// add header
                              {"k2eg-ser-type", serialization_to_string(static_cast<SerializationType>(info_topic->cmd.event_serialization))}});
    }
  }
  publisher->flush(100);
}
#pragma endregion MonitorMessage