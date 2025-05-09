#include "k2eg/controller/command/cmd/SnapshotCommand.h"
#include <k2eg/common/BaseSerialization.h>
#include <k2eg/common/utility.h>

#include <k2eg/service/ServiceResolver.h>
#include <k2eg/service/epics/EpicsData.h>

#include <k2eg/controller/node/worker/SnapshotCommandWorker.h>
#include <k2eg/controller/node/worker/snapshot/ContinuousSnapshotManager.h>

#include <memory>
#include <regex>
#include <algorithm>

using namespace k2eg::controller::command::cmd;
using namespace k2eg::controller::node::worker;
using namespace k2eg::controller::node::worker::snapshot;

using namespace k2eg::service;
using namespace k2eg::service::log;
using namespace k2eg::service::pubsub;
using namespace k2eg::service::epics_impl;

#define GET_QUEUE_FROM_SNAPSHOT_NAME(snapshot_name) ([](const std::string& name) { \
    std::string norm = std::regex_replace(name, std::regex(R"([^A-Za-z0-9\-])"), "_"); \
    std::transform(norm.begin(), norm.end(), norm.begin(), ::tolower); \
    return norm; \
})(snapshot_name)

void set_snapshot_thread_name(const std::size_t idx)
{
    const std::string name = "Snapshot " + std::to_string(idx);
    const bool        result = BS::this_thread::set_os_thread_name(name);
}

ContinuousSnapshotManager::ContinuousSnapshotManager(k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager)
    : logger(ServiceResolver<ILogger>::resolve()), publisher(ServiceResolver<IPublisher>::resolve()), epics_service_manager(epics_service_manager), thread_pool(std::make_shared<BS::light_thread_pool>(std::thread::hardware_concurrency(), set_snapshot_thread_name))
{
    // initialize the local cache
    global_cache_.clear();
    // add epics manager monitor handler
    epics_handler_token = epics_service_manager->addHandler(std::bind(&ContinuousSnapshotManager::epicsMonitorEvent, this, std::placeholders::_1));
    // set the publisher callback
    publisher->setCallBackForReqType("repeating-snapshot-events", std::bind(&ContinuousSnapshotManager::publishEvtCB, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    // set the run flag to true
    run_flag = true;
}

ContinuousSnapshotManager::~ContinuousSnapshotManager()
{
    // set the run flag to false
    run_flag = false;
    // remove epics monitor handler
    epics_handler_token.reset();
    // stop all the thread
    thread_pool->wait();
    // clear the global cache
    global_cache_.clear();
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
    default:
        {
            logger->logMessage(STRING_FORMAT("Command type %1% not supported by this worker", command->type), LogLevel::ERROR);
            break;
        }
    }
}

void ContinuousSnapshotManager::startSnapshot(command::cmd::ConstRepeatingSnapshotCommandShrdPtr snapsthot_command)
{
    bool faulty = false;
    logger->logMessage(STRING_FORMAT("Perepare continuous snapshot ops for '%1%' on topic %2% with sertype: %3%",
                                     snapsthot_command->snapshot_name % snapsthot_command->reply_topic %
                                         serialization_to_string(snapsthot_command->serialization)),
                       LogLevel::DEBUG);
    // create the commmand operation info structure
    auto s_op_ptr = MakeRepeatingSnapshotOpInfoShrdPtr(GET_QUEUE_FROM_SNAPSHOT_NAME(snapsthot_command->snapshot_name), snapsthot_command);

    // chekc if it is already spinned
    {
        std::unique_lock write_lock(snapshot_runinnig_mutex_);
        if (snapshot_runinnig_.find(s_op_ptr->queue_name) != snapshot_runinnig_.end())
        {
            manageReply(-1, STRING_FORMAT("Snapshot %1% is already running", s_op_ptr->cmd->snapshot_name), snapsthot_command);
            return;
        }
    }

    // check if all the command infromation are filled
    if (s_op_ptr->cmd->pv_name_list.empty())
    {
        manageReply(-2, STRING_FORMAT("PV name list is empty for snapshot %1%", s_op_ptr->cmd->snapshot_name), snapsthot_command);
        return;
    }
    if (s_op_ptr->cmd->repeat_delay_msec < 0)
    {
        manageReply(-3, STRING_FORMAT("The repeat delay is not valid %1%", s_op_ptr->cmd->snapshot_name), snapsthot_command);
        return;
    }
    if (s_op_ptr->cmd->time_window_msec < 0)
    {
        manageReply(-4, STRING_FORMAT("The time window is not valid %1%", s_op_ptr->cmd->snapshot_name), snapsthot_command);
        return;
    }
    if (s_op_ptr->cmd->snapshot_name.empty())
    {
        manageReply(-5, STRING_FORMAT("The snapshot name is not valid %1%", s_op_ptr->cmd->snapshot_name), snapsthot_command);
        return;
    }

    // create read lock toa ccess the globa cache to add pv not yet in the cache
    std::shared_lock read_lock(global_cache_mutex_);

    // prepare global cache with all the needed pv
    for (const auto& pv_uri : s_op_ptr->cmd->pv_name_list)
    {
        // get pv name saniutization
        auto sanitized_pv_name = epics_service_manager->sanitizePVName(pv_uri);

        // try to get cache lement on pv name
        auto it = global_cache_.find(sanitized_pv_name->name);

        if (it == global_cache_.end())
        {

            if (!sanitized_pv_name)
            {
                manageReply(-1, STRING_FORMAT("PV '%1%'name malformed", pv_uri), snapsthot_command);
                faulty = true;
                break;
            }
            read_lock.unlock();
            {
                std::unique_lock write(global_cache_mutex_); // take exclusive
                // create entry in cache using the pv_name without the protocol
                auto inserted = global_cache_.emplace(sanitized_pv_name->name, std::make_shared<AtomicMonitorEventShrdPtr>(std::make_shared<MonitorEvent>()));
                if (!inserted.second)
                {
                    manageReply(
                        -2,
                        STRING_FORMAT("Failing to add PV %1% to the global cache fro snapshot", pv_uri % s_op_ptr->cmd->snapshot_name), snapsthot_command);
                    faulty = true;
                    break;
                }
                it = inserted.first;

                // no we can start the monitor for the new registered pv and make it sticky (to not to be removed during
                // monitor cleanup)
                epics_service_manager->monitorChannel(pv_uri, true);
            }
            read_lock.lock();
        }

        // it now is valid
        if (!s_op_ptr->snapshot_views_.emplace(it->first, it->second).second)
        {
            manageReply(
                -3, STRING_FORMAT("Failing to add PV %1% to the view cache for snapshot ", pv_uri % s_op_ptr->cmd->snapshot_name), snapsthot_command);
            faulty = true;
            break;
        }
    }
    if (!faulty)
    {
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
        thread_pool->detach_task(
            [this, s_op_ptr]()
            {
                logger->logMessage(STRING_FORMAT("Start snapshot %1%", s_op_ptr->cmd->snapshot_name), LogLevel::INFO);
                this->processSnapshot(s_op_ptr);
            });

        // return reply to app for submitted command
        manageReply(0, STRING_FORMAT("Start snapshot %1% has been started", s_op_ptr->cmd->snapshot_name), snapsthot_command);
    }
    // add the snapshot to the stopped snapshot list
    {
        std::unique_lock write_lock(snapshot_runinnig_mutex_);
        snapshot_runinnig_.emplace(s_op_ptr->queue_name, RunningInfo{true});
    }
}

void ContinuousSnapshotManager::stopSnapshot(command::cmd::ConstRepeatingSnapshotStopCommandShrdPtr snapsthot_stop_command)
{
    // queue name, the nromalized version of the snapshot name is used has key to stop the snapshot
    auto queue_name = GET_QUEUE_FROM_SNAPSHOT_NAME(snapsthot_stop_command->snapshot_name);

    // we can set to stop
    logger->logMessage(STRING_FORMAT("Try to set snapshot %1% top stop", snapsthot_stop_command->snapshot_name), LogLevel::INFO);
    if (snapsthot_stop_command->snapshot_name.empty())
    {
        manageReply(-1, "", snapsthot_stop_command);
        return;
    }
    {
        // acquire the write lock to stop the snapshot
        std::unique_lock write_lock(snapshot_runinnig_mutex_);
        // check if the snapshot is already stopped
        if (auto it = snapshot_runinnig_.find(queue_name); it != snapshot_runinnig_.end())
        {
            // set snaphsot as to stop
            it->second.is_running = false;
            // send reply to app for submitted command
            manageReply(0, STRING_FORMAT("Snapshot %1% has been stopped", snapsthot_stop_command->snapshot_name), snapsthot_stop_command);
        }
        else
        {
            manageReply(-1, STRING_FORMAT("Snapshot %1% is not running", snapsthot_stop_command->snapshot_name), snapsthot_stop_command);
            return;
        }
    }
}

void ContinuousSnapshotManager::epicsMonitorEvent(EpicsServiceManagerHandlerParamterType event_received)
{
    // prepare reading lock
    std::shared_lock read_lock(global_cache_mutex_);

    for (auto& event : *event_received->event_fail)
    {
        // set the channel as not active
        auto it = global_cache_.find(event->channel_data.pv_name);
        if (it != global_cache_.end())
        {
            it->second->store(nullptr, std::memory_order_release);
        }
    }

    // manage the received data
    for (auto& event : *event_received->event_data)
    {
        const std::string pv_name = event->channel_data.pv_name;
        // store event on the global cache using the atomic pointer
        auto it = global_cache_.find(pv_name);
        if (it != global_cache_.end())
        {
            it->second->store(event, std::memory_order_release);
        }
        else
        {
            logger->logMessage(STRING_FORMAT("Fail to find PV %1% in the global cache", pv_name), LogLevel::ERROR);
        }
    }
}

void ContinuousSnapshotManager::processSnapshot(RepeatingSnapshotOpInfoShrdPtr snapshot_command_info)
{
    // check the run flag
    if (!run_flag)
    {
        return;
    }

    // check if the time window is expired
    if (snapshot_command_info->isTimeout())
    {
        // before triggering the snapshot we need to check if the snapshot is still running
        {
            std::shared_lock lock(snapshot_runinnig_mutex_);
            if (auto it = snapshot_runinnig_.find(snapshot_command_info->queue_name);
                it == snapshot_runinnig_.end() || !it->second.is_running)
            {
                logger->logMessage(
                    STRING_FORMAT("Snapshot %1% is stopped and wuill be removed from queue", snapshot_command_info->queue_name), LogLevel::INFO);
                // remove the snapshot from the running list
                snapshot_runinnig_.erase(snapshot_command_info->queue_name);
                return;
            }
        }

        std::vector<k2eg::service::epics_impl::MonitorEventShrdPtr> snapshot_events;
        // Copy all events to ensure snapshot data remains unchanged during publishing
        {
            // lock in read mode the global cache
            std::shared_lock read_lock(global_cache_mutex_);
            // fastly copy the point to pvdata and release the lock
            for (auto& it : snapshot_command_info->snapshot_views_)
            {
                auto event = it.second->load(std::memory_order_acquire);
                if (event)
                {
                    snapshot_events.push_back(event);
                }
            }
        }

        // get timestamp for the snapshot in unix time and utc
        auto timestamp = std::chrono::system_clock::now();
        auto snap_ts = std::chrono::system_clock::to_time_t(timestamp);
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
        for (auto& event : snapshot_events)
        {
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
        // increment the iteration index
        snapshot_command_info->snapshot_iteration_index++;
    }

    // resubmit the snapshot command
    thread_pool->detach_task(
        [this, snapshot_command_info]()
        {
            this->processSnapshot(snapshot_command_info);
        });
    
}

void ContinuousSnapshotManager::manageReply(const std::int8_t error_code, const std::string& error_message, ConstCommandShrdPtr cmd)
{
    logger->logMessage(error_message);
    if (cmd->reply_topic.empty() || cmd->reply_id.empty())
    {
        logger->logMessage(error_message, LogLevel::ERROR);
        return;
    }
    else
    {
        auto serialized_message = serialize(ContinuousSnapshotCommandReply{error_code, cmd->reply_id, error_message}, cmd->serialization);
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
