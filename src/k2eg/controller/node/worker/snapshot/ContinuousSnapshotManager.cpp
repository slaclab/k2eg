
#include <k2eg/common/BaseSerialization.h>
#include <k2eg/common/utility.h>
#include <k2eg/controller/node/worker/snapshot/BackTimedBufferedSnapshotOpInfo.h>
#include <k2eg/controller/node/worker/snapshot/SnapshotOpInfo.h>
#include <k2eg/service/metric/INodeControllerMetric.h>

#include <k2eg/service/ServiceResolver.h>
#include <k2eg/service/epics/EpicsData.h>

#include <k2eg/controller/command/cmd/SnapshotCommand.h>
#include <k2eg/controller/node/worker/SnapshotCommandWorker.h>
#include <k2eg/controller/node/worker/snapshot/ContinuousSnapshotManager.h>
#include <k2eg/controller/node/worker/snapshot/SnapshotRepeatingOpInfo.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <mutex>
#include <regex>

using namespace k2eg::controller::command::cmd;
using namespace k2eg::controller::node::worker;
using namespace k2eg::controller::node::worker::snapshot;

using namespace k2eg::service;
using namespace k2eg::service::log;
using namespace k2eg::service::pubsub;
using namespace k2eg::service::metric;
using namespace k2eg::service::scheduler;
using namespace k2eg::service::epics_impl;

#define CSM_STAT_TASK_NAME "csm-stat-task"
#define CSM_STAT_TASK_CRON "* * * * * *"

#define GET_QUEUE_FROM_SNAPSHOT_NAME(snapshot_name) ([](const std::string& name) { \
    std::string norm = std::regex_replace(name, std::regex(R"([^A-Za-z0-9\-])"), "_"); \
    std::transform(norm.begin(), norm.end(), norm.begin(), ::tolower); \
    return norm; \
})(snapshot_name)

void set_snapshot_thread_name(const std::size_t idx)
{
    const std::string name = "Repeating Snapshot " + std::to_string(idx);
    const bool        result = BS::this_thread::set_os_thread_name(name);
}

ContinuousSnapshotManager::ContinuousSnapshotManager(k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager)
    : logger(ServiceResolver<ILogger>::resolve())
    , publisher(ServiceResolver<IPublisher>::resolve())
    , epics_service_manager(epics_service_manager)
    , thread_pool(std::make_shared<BS::light_thread_pool>(1, set_snapshot_thread_name))
    , metrics(ServiceResolver<IMetricService>::resolve()->getNodeControllerMetric())
{
    // add epics manager monitor handler
    epics_handler_token = epics_service_manager->addHandler(std::bind(&ContinuousSnapshotManager::epicsMonitorEvent, this, std::placeholders::_1));
    // set the publisher callback
    publisher->setCallBackForReqType("repeating-snapshot-events", std::bind(&ContinuousSnapshotManager::publishEvtCB, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    // set the run flag to true
    run_flag = true;

    // init thread_throttling_vector vector
    auto thread_count = thread_pool->get_thread_count();
    for (size_t i = 0; i < thread_count; ++i)
    {
        thread_throttling_vector.emplace_back(std::make_unique<k2eg::common::ThrottlingManager>());
    }

    // add tak to manage the statistic
    auto statistic_task = MakeTaskShrdPtr(CSM_STAT_TASK_NAME, // name of the task
                                          CSM_STAT_TASK_CRON, // cron expression
                                          std::bind(&ContinuousSnapshotManager::handleStatistic, this, std::placeholders::_1), // task handler
                                          -1 // start at application boot time
    );
    ServiceResolver<Scheduler>::resolve()->addTask(statistic_task);
}

ContinuousSnapshotManager::~ContinuousSnapshotManager()
{
    logger->logMessage("[ContinuousSnapshotManager] Stopping continuous snapshot manager");
    // set the run flag to false
    run_flag = false;
    // remove epics monitor handler
    epics_handler_token.reset();
    // stop all the thread
    thread_pool->wait();
    bool erased = ServiceResolver<Scheduler>::resolve()->removeTaskByName(CSM_STAT_TASK_NAME);
    logger->logMessage(STRING_FORMAT("[ContinuousSnapshotManager] Remove statistic task : %1%", erased));
    logger->logMessage("[ContinuousSnapshotManager] Continuous snapshot manager stopped");
}

std::size_t ContinuousSnapshotManager::getRunningSnapshotCount() const
{
    return thread_pool->get_tasks_running() + thread_pool->get_tasks_queued();
}

void ContinuousSnapshotManager::publishEvtCB(pubsub::EventType type, PublishMessage* const msg, const std::string& error_message)
{
    switch (type)
    {
    case OnDelivery: break;
    case OnSent: break;
    case OnError:
        {
            logger->logMessage(STRING_FORMAT("[ContinuousSnapshotManager::publishEvtCB] %1%", error_message), LogLevel::ERROR);
            break;
        }
    }
}

void ContinuousSnapshotManager::submitSnapshot(ConstCommandShrdPtr command)
{
    switch (command->type)
    {
    case CommandType::repeating_snapshot:
        {
            // forward the command to the continuous snapshot manager
            startSnapshot(static_pointer_cast<const RepeatingSnapshotCommand>(command));
            break;
        }
    case CommandType::repeating_snapshot_stop:
        {
            // forward the command to the continuous snapshot manager
            stopSnapshot(static_pointer_cast<const RepeatingSnapshotStopCommand>(command));
            break;
        }
    case CommandType::repeating_snapshot_trigger:
        {
            // forward the command to the continuous snapshot manager
            triggerSnapshot(static_pointer_cast<const RepeatingSnapshotTriggerCommand>(command));
            break;
        }
    default:
        {
            logger->logMessage(STRING_FORMAT("Command type %1% not supported by this worker", command->type), LogLevel::ERROR);
            break;
        }
    }
}

void ContinuousSnapshotManager::startSnapshot(command::cmd::ConstRepeatingSnapshotCommandShrdPtr snapsthot_command)
{
    bool                                        faulty = false;
    std::vector<service::epics_impl::PVShrdPtr> sanitized_pv_name_list;
    logger->logMessage(STRING_FORMAT("Perepare continuous(triggered %4%) snapshot ops for '%1%' on topic %2% with " "se" "rt" "yp" "e:" " %" "3" "%",
                                     snapsthot_command->snapshot_name % snapsthot_command->reply_topic %
                                         serialization_to_string(snapsthot_command->serialization) % snapsthot_command->triggered),
                       LogLevel::DEBUG);
    // create the commmand operation info structure

    SnapshotOpInfoShrdPtr s_op_ptr = nullptr;
    switch (snapsthot_command->type)
    {
    case SnapshotType::unknown:
        {
            logger->logMessage(STRING_FORMAT("The snapshot type %1% is not supported", snapsthot_command->type), LogLevel::ERROR);
            manageReply(-1, STRING_FORMAT("The snapshot type %1% is not supported", snapsthot_command->type), snapsthot_command);
            return;
        }
    case SnapshotType::NORMAL:
        {
            logger->logMessage(STRING_FORMAT("Create normal snapshot %1%", snapsthot_command->snapshot_name), LogLevel::DEBUG);
            s_op_ptr = MakeSnapshotRepeatingOpInfoShrdPtr(GET_QUEUE_FROM_SNAPSHOT_NAME(snapsthot_command->snapshot_name), snapsthot_command);
            break;
        }
    case SnapshotType::TIMED_BUFFERED:
        {
            logger->logMessage(STRING_FORMAT("Create timed buffered snapshot %1%", snapsthot_command->snapshot_name), LogLevel::DEBUG);
            s_op_ptr = MakeBackTimedBufferedSnapshotOpInfoShrdPtr(GET_QUEUE_FROM_SNAPSHOT_NAME(snapsthot_command->snapshot_name), snapsthot_command);
            break;
        }
    }

    // chekc if it is already spinned
    {
        std::unique_lock write_lock(snapshot_runinnig_mutex_);
        if (snapshot_runinnig_.find(s_op_ptr->queue_name) != snapshot_runinnig_.end())
        {
            manageReply(-2, STRING_FORMAT("Snapshot %1% is already running", s_op_ptr->cmd->snapshot_name), snapsthot_command);
            return;
        }
    }

    // check if all the command infromation are filled
    if (s_op_ptr->cmd->pv_name_list.empty())
    {
        manageReply(-3, STRING_FORMAT("PV name list is empty for snapshot %1%", s_op_ptr->cmd->snapshot_name), snapsthot_command);
        return;
    }
    if (s_op_ptr->cmd->repeat_delay_msec < 0)
    {
        manageReply(-4, STRING_FORMAT("The repeat delay is not valid %1%", s_op_ptr->cmd->snapshot_name), snapsthot_command);
        return;
    }
    if (s_op_ptr->cmd->time_window_msec < 0)
    {
        manageReply(-5, STRING_FORMAT("The time window is not valid %1%", s_op_ptr->cmd->snapshot_name), snapsthot_command);
        return;
    }
    if (s_op_ptr->cmd->snapshot_name.empty())
    {
        manageReply(-6, STRING_FORMAT("The snapshot name is not valid %1%", s_op_ptr->cmd->snapshot_name), snapsthot_command);
        return;
    }

    for (const auto& pv_uri : s_op_ptr->cmd->pv_name_list)
    {
        // get pv name saniutization
        auto sanitized_pv_name = epics_service_manager->sanitizePVName(pv_uri);
        if (!sanitized_pv_name)
        {
            manageReply(-7, STRING_FORMAT("The pv uri %1% is not parsable", pv_uri), snapsthot_command);
            return;
        }
        // add the pv to the list of pv to monitor
        sanitized_pv_name_list.push_back(std::move(sanitized_pv_name));
    }

    // init the snapshot operation info
    if (!s_op_ptr->init(sanitized_pv_name_list))
    {
        manageReply(-8, STRING_FORMAT("The snapshot %1% is not valid", s_op_ptr->cmd->snapshot_name), snapsthot_command);
        return;
    }

    // create topic where publish the snapshot
    publisher->createQueue(QueueDescription{
        .name = s_op_ptr->queue_name,
        .paritions = 3,
        .replicas = 1, // put default to 1 need to be calculated with more compelx logic for higher values
        .retention_time = 1000 * 60 * 60,
        .retention_size = 1024 * 1024 * 50,
    });

    // we got no faulty during the cache preparation
    // so we can start the snapshot
    // add the snapshot to the stopped snapshot list and to the pv list
    {
        std::unique_lock write_lock(snapshot_runinnig_mutex_);
        // assocaite the topi to the snapshot in the running list
        snapshot_runinnig_.emplace(s_op_ptr->queue_name, s_op_ptr);
        for (auto& pv_uri : sanitized_pv_name_list)
        {
            logger->logMessage(STRING_FORMAT("associate snapshot '%1%' to pv '%2%'", s_op_ptr->cmd->snapshot_name % pv_uri), LogLevel::DEBUG);
            // associate snaphsot to each pv, so the epics handler can
            // find the snapshot to fiil with data
            pv_snapshot_map_.emplace(std::make_pair(pv_uri->name, s_op_ptr));
        }
    }

    // start monitor for all needed pv
    for (auto& pv_uri : s_op_ptr->cmd->pv_name_list)
    {
        logger->logMessage(STRING_FORMAT("PV '%1%' enabling monitor", pv_uri), LogLevel::DEBUG);
        epics_service_manager->monitorChannel(pv_uri, true);
    }

    // submite snapshot to the thread pool
    thread_pool->detach_task(
        [this, s_op_ptr]()
        {
            logger->logMessage(STRING_FORMAT("Start snapshot %1%", s_op_ptr->cmd->snapshot_name), LogLevel::INFO);
            this->processSnapshot(std::static_pointer_cast<SnapshotOpInfo>(s_op_ptr));
        });

    // return reply to app for submitted command
    manageReply(0, STRING_FORMAT("The snapshot '%1%' has been started", s_op_ptr->cmd->snapshot_name), snapsthot_command,
                s_op_ptr->queue_name);
}

void ContinuousSnapshotManager::triggerSnapshot(command::cmd::ConstRepeatingSnapshotTriggerCommandShrdPtr snapshot_trigger_command)
{
    // we can set to stop
    logger->logMessage(STRING_FORMAT("Try to set snapshot '%1%' to stop", snapshot_trigger_command->snapshot_name), LogLevel::INFO);
    if (snapshot_trigger_command->snapshot_name.empty())
    {
        manageReply(-1, "The snapshot name is empty", snapshot_trigger_command);
        return;
    }
    {
        // queue name, the nromalized version of the snapshot name is used has key to stop the snapshot
        auto queue_name = GET_QUEUE_FROM_SNAPSHOT_NAME(snapshot_trigger_command->snapshot_name);
        // acquire the write lock to stop the snapshot
        std::unique_lock write_lock(snapshot_runinnig_mutex_);
        // check if the snapshot is already stopped
        auto it = snapshot_runinnig_.find(queue_name);
        if (it != snapshot_runinnig_.end())
        {
            // set snaphsot as to stop
            it->second->request_to_trigger = true;
            // send reply to app for submitted command
            manageReply(0, STRING_FORMAT("Snapshot '%1%' has been armed", snapshot_trigger_command->snapshot_name), snapshot_trigger_command);
        }
        else
        {
            manageReply(-2, STRING_FORMAT("Snapshot '%1%' has not been found", snapshot_trigger_command->snapshot_name), snapshot_trigger_command);
            return;
        }
    }
}

void ContinuousSnapshotManager::stopSnapshot(command::cmd::ConstRepeatingSnapshotStopCommandShrdPtr snapsthot_stop_command)
{
    // queue name, the nromalized version of the snapshot name is used has key to stop the snapshot
    auto queue_name = GET_QUEUE_FROM_SNAPSHOT_NAME(snapsthot_stop_command->snapshot_name);

    // we can set to stop
    logger->logMessage(STRING_FORMAT("Try to set snapshot '%1%' to stop", snapsthot_stop_command->snapshot_name), LogLevel::INFO);
    if (snapsthot_stop_command->snapshot_name.empty())
    {
        manageReply(-1, "The snapshot name is empty", snapsthot_stop_command);
        return;
    }
    {
        // acquire the write lock to stop the snapshot
        std::unique_lock write_lock(snapshot_runinnig_mutex_);
        // check if the snapshot is already stopped
        auto it = snapshot_runinnig_.find(queue_name);
        if (it != snapshot_runinnig_.end())
        {
            // set snaphsot as to stop
            it->second->is_running = false;
            // send reply to app for submitted command
            manageReply(0, STRING_FORMAT("Snapshot '%1%' has been stopped", snapsthot_stop_command->snapshot_name), snapsthot_stop_command);
        }
        else
        {
            manageReply(0, STRING_FORMAT("Snapshot '%1%' is already stopped", snapsthot_stop_command->snapshot_name), snapsthot_stop_command);
            return;
        }
    }
}

void ContinuousSnapshotManager::epicsMonitorEvent(EpicsServiceManagerHandlerParamterType event_received)
{
    // prepare reading lock
    std::shared_lock read_lock(snapshot_runinnig_mutex_);

    // for (auto& event : *event_received->event_fail)
    // {
    //     // set the channel as not active
    //     auto it = snapshot_runinnig_.find(event->channel_data.pv_name);
    //     if (it != snapshot_runinnig_.end())
    //     {
    //         it->second->addData(nullptr, std::memory_order_release);
    //     }
    // }

    // manage the received data
    for (auto& event : *event_received->event_data)
    {
        const std::string pv_name = event->channel_data.pv_name;
        // store event on the global cache using the atomic pointer
        auto range = pv_snapshot_map_.equal_range(pv_name);
        for (auto it = range.first; it != range.second; ++it)
        {
            if (it->second)
            {
                it->second->addData(event);
            }
        }
    }
}

void ContinuousSnapshotManager::processSnapshot(SnapshotOpInfoShrdPtr snapshot_command_info)
{
    // check the run flag
    if (!run_flag)
    {
        return;
    }

    const auto thread_index = BS::this_thread::get_index();
    if (!thread_index.has_value())
        return;
    auto& throttling = thread_throttling_vector[thread_index.value()];
    // check if the time window is expired
    if (snapshot_command_info->isTimeout())
    {
        throttling->update(true);
        // before triggering the snapshot we need to check if the snapshot is still running
        if (!snapshot_command_info->is_running)
        {
            // we need to stop this trigger
            std::unique_lock lock(snapshot_runinnig_mutex_);
            if (auto it = snapshot_runinnig_.find(snapshot_command_info->queue_name); it != snapshot_runinnig_.end())
            {

                logger->logMessage(
                    STRING_FORMAT("Snapshot %1% is stopped and will be removed from queue", snapshot_command_info->queue_name), LogLevel::INFO);
                // remove the snapshot from the running list
                snapshot_runinnig_.erase(snapshot_command_info->queue_name);
                // erase the snapshot from the pv_snapshot_map_
                logger->logMessage(
                    STRING_FORMAT("Remove Snapshot %1% from all his pv snapshot map", snapshot_command_info->cmd->snapshot_name), LogLevel::INFO);
                for (auto& pv_uri : snapshot_command_info->cmd->pv_name_list)
                {
                    auto s_pv = epics_service_manager->sanitizePVName(pv_uri);
                    auto range = pv_snapshot_map_.equal_range(s_pv->name);
                    for (auto it = range.first; it != range.second; ++it)
                    {
                        if (it->second == snapshot_command_info)
                        {
                            logger->logMessage(
                                STRING_FORMAT("Snapshot '%1%' is removed from PV mapa %2%", snapshot_command_info->cmd->snapshot_name % s_pv), LogLevel::INFO);
                            pv_snapshot_map_.erase(it);
                            break;
                        }
                    }

                    // remove instance of epics channel
                    logger->logMessage(STRING_FORMAT("PV '%1%' disabling monitor", pv_uri), LogLevel::DEBUG);
                    epics_service_manager->monitorChannel(pv_uri, false);
                }

                logger->logMessage(STRING_FORMAT("Snapshot %1% is cancelled", snapshot_command_info->queue_name), LogLevel::INFO);
            }
            return;
        }

        // increment the iteration index
        snapshot_command_info->snapshot_iteration_index++;
        // get timestamp for the snapshot in unix time and utc
        std::int64_t snap_ts =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        {
            auto serialized_header_message = serialize(
                RepeatingSnaptshotHeader{0, snapshot_command_info->cmd->snapshot_name, snap_ts, snapshot_command_info->snapshot_iteration_index},
                snapshot_command_info->cmd->serialization);
            // send the header for the snapshot
            publisher->pushMessage(MakeReplyPushableMessageUPtr(snapshot_command_info->queue_name, "repeating-snapshot-events",
                                                                snapshot_command_info->cmd->snapshot_name, serialized_header_message),
                                   {{"k2eg-ser-type", serialization_to_string(snapshot_command_info->cmd->serialization)}});
        }

        // snapshot event vector contains the data from all pv
        // and the smart poiner are no more changed so we can push safetely
        // and we have all non null pointer here
        int elementsIndex = 0;
        // get data from snapshot command info
        auto snapshot_events = snapshot_command_info->getData();
        for (auto& event : snapshot_events)
        {
            logger->logMessage(STRING_FORMAT("PV '%1%' ready to be submitted with snapshot %2%",
                                             event->channel_data.pv_name % snapshot_command_info->cmd->snapshot_name),
                               LogLevel::DEBUG);
            auto serialized_message = serialize(RepeatingSnaptshotData{1, snap_ts, snapshot_command_info->snapshot_iteration_index,
                                                                       MakeChannelDataShrdPtr(event->channel_data)},
                                                snapshot_command_info->cmd->serialization);
            if (serialized_message)
            {
                // publish the data
                publisher->pushMessage(MakeReplyPushableMessageUPtr(snapshot_command_info->queue_name, "repeating-snapshot-events",
                                                                    snapshot_command_info->cmd->snapshot_name, serialized_message),
                                       {{"k2eg-ser-type", serialization_to_string(snapshot_command_info->cmd->serialization)}});
            }
            else
            {
                logger->logMessage(STRING_FORMAT("Failing serializing snapshot %1% for PV %2%",
                                                 snapshot_command_info->cmd->snapshot_name % event->channel_data.pv_name),
                                   LogLevel::ERROR);
            }
        }

        // send completion for this snapshot submission
        auto serialized_completion_message = serialize(RepeatingSnaptshotCompletion{2, 0, "", snapshot_command_info->cmd->snapshot_name, snap_ts,
                                                                                    snapshot_command_info->snapshot_iteration_index},
                                                       snapshot_command_info->cmd->serialization);
        // publish the data
        publisher->pushMessage(MakeReplyPushableMessageUPtr(snapshot_command_info->queue_name, "repeating-snapshot-events",
                                                            snapshot_command_info->cmd->snapshot_name, serialized_completion_message),
                               {{"k2eg-ser-type", serialization_to_string(snapshot_command_info->cmd->serialization)}});
        logger->logMessage(STRING_FORMAT("Snapshot %1% iteration %2% fired with data [%3%]", snapshot_command_info->cmd->snapshot_name %
                                                                                                 snapshot_command_info->snapshot_iteration_index %
                                                                                                 snapshot_events.size()),
                           LogLevel::DEBUG);
    }

    throttling->update(false);

    // resubmit the snapshot command
    thread_pool->detach_task(
        [this, snapshot_command_info]()
        {
            this->processSnapshot(snapshot_command_info);
        });

    // give some time to relax
    std::this_thread::sleep_for(std::chrono::microseconds(100));
}

void ContinuousSnapshotManager::manageReply(const std::int8_t error_code, const std::string& error_message, ConstCommandShrdPtr cmd, const std::string& publishing_topic)
{
    logger->logMessage(error_message, error_code == 0 ? LogLevel::INFO : LogLevel::ERROR);
    {
        auto serialized_message = serialize(ContinuousSnapshotCommandReply{error_code, cmd->reply_id, error_message, publishing_topic}, cmd->serialization);
        if (!serialized_message)
        {
            logger->logMessage("Invalid serialized message", LogLevel::FATAL);
        }
        else
        {
            publisher->pushMessage(MakeReplyPushableMessageUPtr(cmd->reply_topic, "repeating-snapshot-events", "repeating-snapshot-events", serialized_message), {{"k2eg-ser-type", serialization_to_string(cmd->serialization)}});
        }
    }
}

void ContinuousSnapshotManager::handleStatistic(TaskProperties& task_properties)
{
    // update throttle metric
    // Assuming you have a metric object, otherwise replace 'metric' with the correct instance or accessor
    for (size_t i = 0; i < thread_throttling_vector.size(); ++i)
    {
        auto&       throttling = thread_throttling_vector[i];
        std::string thread_id = std::to_string(i);
        auto        t_stat = throttling->getStats();
        metrics.incrementCounter(k2eg::service::metric::INodeControllerMetricCounterType::ThrottlingIdleCounter, t_stat.idle_counter, {{"thread_id", thread_id}});
        metrics.incrementCounter(k2eg::service::metric::INodeControllerMetricCounterType::ThrottlingEventCounter, t_stat.total_events_processed, {{"thread_id", thread_id}});
        metrics.incrementCounter(
            k2eg::service::metric::INodeControllerMetricCounterType::ThrottlingDurationCounter, t_stat.total_idle_cycles, {{"thread_id", thread_id}});
        metrics.incrementCounter(k2eg::service::metric::INodeControllerMetricCounterType::ThrottleGauge, t_stat.throttle_ms, {{"thread_id", thread_id}});
    }
}