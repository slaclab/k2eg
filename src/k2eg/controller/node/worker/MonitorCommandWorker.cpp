#include <k2eg/common/utility.h>

#include <k2eg/controller/command/cmd/Command.h>
#include <k2eg/controller/node/worker/CommandWorker.h>
#include <k2eg/controller/command/cmd/MonitorCommand.h>
#include <k2eg/controller/node/worker/MonitorCommandWorker.h>
#include <k2eg/controller/node/worker/monitor/MonitorChecker.h>

#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/scheduler/Task.h>
#include <k2eg/service/ServiceResolver.h>
#include <k2eg/service/pubsub/IPublisher.h>
#include <k2eg/service/scheduler/Scheduler.h>
#include <k2eg/service/metric/IMetricService.h>

#include <mutex>
#include <execution>
#include <functional>
#include <shared_mutex>

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
      starting_up(true),
      run_rate_thread(false) {
  // reset all processed element present in the monitor database
  monitor_checker_shrd_ptr->resetMonitorToProcess();
  // add epics monitor handler
  epics_handler_token = epics_service_manager->addHandler(std::bind(&MonitorCommandWorker::epicsMonitorEvent, this, std::placeholders::_1));
  // add monitor checker handler
  monitor_checker_token = monitor_checker_shrd_ptr->addHandler(std::bind(&MonitorCommandWorker::handleMonitorCheckEvents, this, std::placeholders::_1));

  auto task_restart_monitor = MakeTaskShrdPtr(STARTUP_MONITOR_TASK_NAME,
                                              STARTUP_MONITOR_TASK_NAME_CRON,
                                              std::bind(&MonitorCommandWorker::handleRestartMonitorTask, this, std::placeholders::_1),
                                              -1  // start at application boot time
  );
  ServiceResolver<Scheduler>::resolve()->addTask(task_restart_monitor);

  publisher->setCallBackForReqType(
    "monitor-message", 
    std::bind(
      &MonitorCommandWorker::publishEvtCB, 
      this, 
      std::placeholders::_1,
      std::placeholders::_2,
      std::placeholders::_3)
    );
}

MonitorCommandWorker::~MonitorCommandWorker() {
  // dispose the token for the event
  epics_handler_token.reset();
  monitor_checker_token.reset();
  logger->logMessage("[ Shoutdown ] Stop metrics update thread");

  // remove automated task from the scheduler
  logger->logMessage("[ Shoutdown ] Remove periodic task from scheduler");
  bool erased = ServiceResolver<Scheduler>::resolve()->removeTaskByName(MAINTANACE_TASK_NAME);
  logger->logMessage(STRING_FORMAT("Remove periodic maintanance : %1%", erased));
  erased = ServiceResolver<Scheduler>::resolve()->removeTaskByName(STARTUP_MONITOR_TASK_NAME);
  logger->logMessage(STRING_FORMAT("Remove startup task: %1%", erased));

  // stop metric monitor
  run_rate_thread = false;
  if(rate_thread.joinable()) rate_thread.join();

  // dipose all still live monitor
  logger->logMessage("[ Dispose Worker ] stop all still live monitor");
  for (auto& mon_vec_for_pv : channel_topics_map) {
    logger->logMessage(STRING_FORMAT("[ Exing Worker ] Stop all monitor for pv '%1%'", mon_vec_for_pv.first));
    for (auto& monitor_info : mon_vec_for_pv.second.cmd_vec) {
      logger->logMessage(
          STRING_FORMAT("[ Dispose Worker ] Stop monitor for pv '%1%' with target '%2%'", monitor_info.pv_name % monitor_info.channel_destination));
      epics_service_manager->monitorChannel(monitor_info.pv_name, false);
    }
  }
}

void
MonitorCommandWorker::handleRestartMonitorTask(TaskProperties& task_properties) {
  std::lock_guard<std::mutex> lock(periodic_task_mutex);
  logger->logMessage("[ Startup Task ] Restart monitor requests");
  task_properties.completed = !(starting_up = monitor_checker_shrd_ptr->scanForRestart());
  task_properties.run_asap  = true;
  if (!starting_up) {
    logger->logMessage("[ Startup Task ] Startup completed");
    // start checker timing
    auto task_periodic_maintanance = MakeTaskShrdPtr(MAINTANACE_TASK_NAME,
                                                     monitor_command_configuration.cron_scheduler_monitor_check,
                                                     std::bind(&MonitorCommandWorker::handlePeriodicTask, this, std::placeholders::_1));
    ServiceResolver<Scheduler>::resolve()->addTask(task_periodic_maintanance);

    // activate thread for metric
    logger->logMessage("[ Startup Task ] Startup thread for udpate pv metrics");
    run_rate_thread = true;
    start_sample_ts = std::chrono::steady_clock::now();
    rate_thread     = std::thread(&MonitorCommandWorker::calcPVCount, this);
  }
}

void
MonitorCommandWorker::handlePeriodicTask(TaskProperties& task_properties) {
  std::lock_guard<std::mutex> lock(periodic_task_mutex);
  logger->logMessage("[ Automatic Task ] Checking active monitor");
  auto processed = monitor_checker_shrd_ptr->scanForMonitorToStop();
  if (!processed) {
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
MonitorCommandWorker::publishEvtCB(pubsub::EventType type, PublishMessage* const msg, const std::string& error_message){
  switch (type) {
    case OnDelivery:break;
    case OnSent:break;
    case OnError: {
      logger->logMessage(STRING_FORMAT("[MonitorCommandWorker::publishEvtCB] %1%", error_message), LogLevel::ERROR);
      break;
    }
  }
}

void
MonitorCommandWorker::handleMonitorCheckEvents(MonitorHandlerData checker_event_data) {
  auto sanitized_pv = epics_service_manager->sanitizePVName(checker_event_data.monitor_type.pv_name);
  if (!sanitized_pv) {
    logger->logMessage(STRING_FORMAT("Error on sanitization for '%1%'", checker_event_data.monitor_type.pv_name));
    return;
  }

  // access map for modify it  // esclusive lock
  std::unique_lock<std::shared_mutex> lock_pv_map(channel_map_mtx);
  auto&                               pv_monitor_info = channel_topics_map[sanitized_pv->name];
  lock_pv_map.unlock();

  switch (checker_event_data.action) {
    case MonitorHandlerAction::Start: {
      logger->logMessage(STRING_FORMAT("Activate monitor on '%1%' for topic '%2%'",
                                       checker_event_data.monitor_type.pv_name % checker_event_data.monitor_type.channel_destination));
      // got start event
      if (std::find_if(std::begin(pv_monitor_info.cmd_vec), std::end(pv_monitor_info.cmd_vec), [&checker_event_data](auto& info_topic) {
            return info_topic.channel_destination.compare(checker_event_data.monitor_type.channel_destination) == 0;
          }) == std::end(pv_monitor_info.cmd_vec)) {
        {
          std::unique_lock<std::shared_mutex> lock_pv_map(channel_map_mtx);
          // add monitor infor to vector of the PV
          pv_monitor_info.cmd_vec.push_back(checker_event_data.monitor_type);
        }

        logger->logMessage(
            STRING_FORMAT("Create topic for '%1%' with name '%2%'", checker_event_data.monitor_type.pv_name % get_queue_for_pv(sanitized_pv->name)));
        publisher->createQueue(QueueDescription{
            // TODO apply somelogic here in future
            .name           = get_queue_for_pv(sanitized_pv->name),
            .paritions      = 3,
            .replicas       = 1, // put default to 1 need to be calculated with more compelx logic for higher values
            .retention_time = 1000 * 60 * 60,
            .retention_size = 1024 * 1024 * 50,
        });
        logger->logMessage(STRING_FORMAT("Start monitoring topic for '%1%'", checker_event_data.monitor_type.pv_name));
        epics_service_manager->monitorChannel(checker_event_data.monitor_type.pv_name, true);
      } else {
        epics_service_manager->forceMonitorChannelUpdate(checker_event_data.monitor_type.pv_name);
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
      auto itr = std::find_if(std::begin(pv_monitor_info.cmd_vec), std::end(pv_monitor_info.cmd_vec), [&checker_event_data](auto& info_topic) {
        return info_topic.channel_destination.compare(checker_event_data.monitor_type.channel_destination) == 0;
      });
      if (itr != std::end(pv_monitor_info.cmd_vec)) {
        {
          std::unique_lock<std::shared_mutex> lock_pv_map(channel_map_mtx);
          pv_monitor_info.cmd_vec.erase(itr);
        }
        epics_service_manager->monitorChannel(checker_event_data.monitor_type.pv_name, false);
        logger->logMessage(STRING_FORMAT("Monitor stopped on '%1%' for topic '%2%'",
                                         checker_event_data.monitor_type.pv_name % checker_event_data.monitor_type.channel_destination));
        if (pv_monitor_info.cmd_vec.size() == 0) {
          // whe have to remove the key because no more monitor has been requested for the pv
          std::unique_lock<std::shared_mutex> lock_pv_map(channel_map_mtx);
          channel_topics_map.erase(sanitized_pv->name);
          logger->logMessage(STRING_FORMAT("Removed pv information for '%1%'", checker_event_data.monitor_type.pv_name));
        }
      } else {
        logger->logMessage(STRING_FORMAT("No active monitor on '%1%' for topic '%2%'",
                                         checker_event_data.monitor_type.pv_name % checker_event_data.monitor_type.channel_destination));
      }
      break;
    }
  }
}

void
MonitorCommandWorker::processCommand(std::shared_ptr<BS::priority_thread_pool> thread_pool, ConstCommandShrdPtr command) {
  if (starting_up) {
    logger->logMessage("[ Starting up ] Comamnd cannot be executed", LogLevel::ERROR);
    manageReply(-2, "Command cannot be executed, k2eg monitor worker is starting", command);
    return;
  }
  if(!has_serialization_for_type(command->serialization)) {
    logger->logMessage(STRING_FORMAT("No serializer found for type %1%", serialization_to_string(command->serialization)), LogLevel::ERROR);
    manageReply(-3, "No serializer found", command);
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
  if (cmd_ptr->monitor_destination_topic.empty()) {
    logger->logMessage(STRING_FORMAT("No destination topic found on monitor command for %1%", cmd_ptr->pv_name), LogLevel::ERROR);
    manageReply(-1, "Empty destination topic", cmd_ptr);
    return;
  }
  monitor_checker_shrd_ptr->storeMonitorData({ChannelMonitorType{.pv_name             = cmd_ptr->pv_name,
                                                                 .event_serialization = static_cast<std::uint8_t>(cmd_ptr->serialization),
                                                                 //  .channel_protocol    = cmd_ptr->protocol,
                                                                 .channel_destination = cmd_ptr->monitor_destination_topic}});
  manageReply(0, STRING_FORMAT("Monitor activated for %1%", cmd_ptr->pv_name), cmd_ptr);
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
    // extract all pv component
    auto sanitized_pv = epics_service_manager->sanitizePVName(pv_name);
    if (!sanitized_pv) {
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
  logger->logMessage(error_message);
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
  logger->logMessage(STRING_FORMAT("Received epics monitor event count:\ndata: %1%\ncancel: %2%\ndisconnect: %3%\nfail: %4%",
                                   event_received->event_data->size() % event_received->event_cancel->size() % event_received->event_disconnect->size() %
                                       event_received->event_fail->size()),
                     LogLevel::TRACE);
#endif
  //----------update metric--------
  metric.incrementCounter(IEpicsMetricCounterType::MonitorData, event_received->event_data->size());
  metric.incrementCounter(IEpicsMetricCounterType::MonitorCancel, event_received->event_cancel->size());
  metric.incrementCounter(IEpicsMetricCounterType::MonitorDisconnect, event_received->event_disconnect->size());
  metric.incrementCounter(IEpicsMetricCounterType::MonitorFail, event_received->event_fail->size());

  // check fail to connect pv
  {
    std::shared_lock slock(channel_map_mtx);
    for (auto& event : *event_received->event_fail) {
      channel_topics_map[event->channel_data.pv_name].active = false;
      logger->logMessage(STRING_FORMAT("PV %1% is not connected", event->channel_data.pv_name), LogLevel::DEBUG);
    }
  }

  // cache the various serilized message for each serializaiton type
  std::map<SerializationType, ConstSerializedMessageShrdPtr> local_serialization_cache;
  for (auto& event : *event_received->event_data) {
    // publisher
    std::shared_lock slock(channel_map_mtx);

    // set channel as active
    if(!channel_topics_map[event->channel_data.pv_name].active) {
      channel_topics_map[event->channel_data.pv_name].active = true;
      logger->logMessage(STRING_FORMAT("PV %1% is connected", event->channel_data.pv_name), LogLevel::DEBUG);
    }
    

    // forward messages
    for (auto& monitor_info : channel_topics_map[event->channel_data.pv_name].cmd_vec) {
      logger->logMessage(STRING_FORMAT("Publish channel %1% on topic %2%", event->channel_data.pv_name % monitor_info.channel_destination), LogLevel::TRACE);
      if (!local_serialization_cache.contains(static_cast<SerializationType>(monitor_info.event_serialization))) {
        // cache new serialized message
        local_serialization_cache[static_cast<SerializationType>(monitor_info.event_serialization)] =
            serialize(event->channel_data, static_cast<SerializationType>(monitor_info.event_serialization));
      }
      publisher->pushMessage(MakeReplyPushableMessageUPtr(monitor_info.channel_destination,
                                                          "monitor-message",
                                                          event->channel_data.pv_name,
                                                          local_serialization_cache[static_cast<SerializationType>(monitor_info.event_serialization)]),
                             {// add header
                              {"k2eg-ser-type", serialization_to_string(static_cast<SerializationType>(monitor_info.event_serialization))}});
    }
  }
  publisher->flush(100);
}

void
MonitorCommandWorker::calcPVCount() {
  while (run_rate_thread) {
    double rate_per_sec = 0;
    auto   end_time     = std::chrono::steady_clock::now();
    auto   elapsed      = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_sample_ts);
    if (elapsed.count() >= 5) {
      start_sample_ts = end_time;
      std::shared_lock slock(channel_map_mtx);
      int              active_pv =
          std::count_if(std::execution::par, channel_topics_map.begin(), channel_topics_map.end(), [](const auto& pair) { return pair.second.active == true; });

      metric.incrementCounter(IEpicsMetricCounterType::ActiveMonitor, active_pv);
      metric.incrementCounter(IEpicsMetricCounterType::TotalMonitor, channel_topics_map.size());
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}
#pragma endregion MonitorMessage