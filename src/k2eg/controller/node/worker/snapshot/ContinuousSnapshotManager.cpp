

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
#include <set>
#include <string>
#include <unistd.h>
#include <utility>

using namespace k2eg::controller::command::cmd;
using namespace k2eg::controller::node::worker;
using namespace k2eg::controller::node::worker::snapshot;

using namespace k2eg::service;
using namespace k2eg::service::log;
using namespace k2eg::service::pubsub;
using namespace k2eg::service::metric;
using namespace k2eg::service::scheduler;
using namespace k2eg::service::epics_impl;

#define GET_QUEUE_FROM_SNAPSHOT_NAME(snapshot_name) ([](const std::string& name) { \
    std::string norm = std::regex_replace(name, std::regex(R"([^A-Za-z0-9\-])"), "_"); \
    std::transform(norm.begin(), norm.end(), norm.begin(), ::tolower); \
    return norm; \
})(snapshot_name)

void set_snapshot_thread_name(const std::size_t idx)
{
    // Use a local variable, not static/global
    std::ostringstream oss;
    oss << "Repeating Snapshot " << idx;
    const std::string name = oss.str();
    BS::this_thread::set_os_thread_name(name);
}

std::string get_pv_names(std::set<std::string> name_set)
{
    std::string all_pv_names;
    for (const auto& pv_name : name_set)
    {
        if (!all_pv_names.empty())
        {
            all_pv_names += ", ";
        }
        all_pv_names += pv_name;
    }
    return all_pv_names;
}

inline auto thread_namer = [](unsigned long idx)
{
    set_snapshot_thread_name(idx);
};

ContinuousSnapshotManager::ContinuousSnapshotManager(const RepeatingSnaptshotConfiguration& repeating_snapshot_configuration, k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager)
    : repeating_snapshot_configuration(repeating_snapshot_configuration)
    , logger(ServiceResolver<ILogger>::resolve())
    , publisher(ServiceResolver<IPublisher>::resolve())
    , epics_service_manager(epics_service_manager)
    , thread_pool(std::make_shared<BS::light_thread_pool>(repeating_snapshot_configuration.snapshot_processing_thread_count, thread_namer))
    , metrics(ServiceResolver<IMetricService>::resolve()->getNodeControllerMetric())
    , iteration_sync_(std::make_unique<SnapshotIterationSynchronizer>())
{
    // add epics manager monitor handler
    epics_handler_token = epics_service_manager->addHandler(std::bind(&ContinuousSnapshotManager::epicsMonitorEvent, this, std::placeholders::_1));
    // set the publisher callback
    publisher->setCallBackForReqType("repeating-snapshot-events", std::bind(&ContinuousSnapshotManager::publishEvtCB, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    // set the run flag to true
    run_flag = true;

    // start expiration checker
    expiration_thread_running = true;
    expiration_thread = std::thread(&ContinuousSnapshotManager::expirationCheckerLoop, this);
    pthread_setname_np(expiration_thread.native_handle(), "SnapshotExpirationChecker");
}

ContinuousSnapshotManager::~ContinuousSnapshotManager()
{
    logger->logMessage("[ContinuousSnapshotManager] Stopping continuous snapshot manager");
    expiration_thread_running = false;
    if (expiration_thread.joinable())
    {
        expiration_thread.join();
    }
    // set the run flag to false
    run_flag = false;
    // remove epics monitor handler
    epics_handler_token.reset();
    // stop all the thread
    thread_pool->wait();
    logger->logMessage("[ContinuousSnapshotManager] Continuous snapshot manager stopped");
}

std::size_t ContinuousSnapshotManager::getRunningSnapshotCount() const
{
    std::unique_lock write_lock(snapshot_runinnig_mutex_);
    // check if the snapshot is already stopped
    return snapshot_runinnig_.size();
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
            manageReply(0, STRING_FORMAT("Snapshot %1% is already running", s_op_ptr->cmd->snapshot_name), snapsthot_command);
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

    for (const auto& pv_uri : s_op_ptr->cmd->pv_name_list)
    {
        // get pv name saniutization
        auto sanitized_pv_name = epics_service_manager->sanitizePVName(pv_uri);
        if (!sanitized_pv_name)
        {
            manageReply(-6, STRING_FORMAT("The pv uri %1% is not parsable", pv_uri), snapsthot_command);
            return;
        }
        // add the pv to the list of pv to monitor
        sanitized_pv_name_list.push_back(std::move(sanitized_pv_name));
    }

    // init the snapshot operation info
    if (!s_op_ptr->init(sanitized_pv_name_list))
    {
        manageReply(-7, STRING_FORMAT("The snapshot %1% is not valid", s_op_ptr->cmd->snapshot_name), snapsthot_command);
        return;
    }

    // create topic where publish the snapshot
    publisher->createQueue(QueueDescription{
        .name = s_op_ptr->queue_name,
        .paritions = 1, // put default to 1 need to be calculated with more complex logic for higher values
        .replicas = 1,  // put default to 1 need to be calculated with more complex logic for higher values
        .retention_time = 1000 * 60 * 60,
        .retention_size = 1024 * 1024 * 50,
    });

    // read metadata to fast the get of topic information
    auto topic_info = publisher->getQueueMetadata(s_op_ptr->queue_name);
    logger->logMessage(STRING_FORMAT("Topic '%1%' metadata => subscriber number:%2%",
                                     s_op_ptr->queue_name % topic_info->subscriber_groups.size()),
                       LogLevel::DEBUG);
    // we got no faulty during the cache preparation
    // so we can start the snapshot
    // add the snapshot to the stopped snapshot list and to the pv list
    {
        std::unique_lock write_lock(snapshot_runinnig_mutex_);
        // assocaite the topi to the snapshot in the running list
        snapshot_runinnig_.emplace(s_op_ptr->queue_name, s_op_ptr);
        for (auto& pv_uri : sanitized_pv_name_list)
        {
            logger->logMessage(
                STRING_FORMAT("associate snapshot '%1%' to pv '%2%'", s_op_ptr->cmd->snapshot_name % pv_uri->name), LogLevel::DEBUG);
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
    std::future<void>               removal_future;
    std::shared_ptr<SnapshotOpInfo> s_to_stop;
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
        std::shared_lock write_lock(snapshot_runinnig_mutex_);
        // check if the snapshot is already stopped
        auto it = snapshot_runinnig_.find(queue_name);
        if (it != snapshot_runinnig_.end())
        {
            // Get the future before we mark it for stopping
            s_to_stop = it->second;
            removal_future = s_to_stop->removal_promise.get_future();
            // set snaphsot as to stop
            it->second->is_running = false;
        }
        else
        {
            manageReply(0, STRING_FORMAT("Snapshot '%1%' is already stopped", snapsthot_stop_command->snapshot_name), snapsthot_stop_command);
            return;
        }
    }

    // Wait for the expirationCheckerLoop to confirm the snapshot has been fully removed.
    if (removal_future.valid())
    {
        logger->logMessage(
            STRING_FORMAT("Waiting for snapshot '%1%' to be fully decommissioned...", snapsthot_stop_command->snapshot_name), LogLevel::DEBUG);
        removal_future.wait();
        logger->logMessage(STRING_FORMAT("Snapshot '%1%' has been fully decommissioned.", snapsthot_stop_command->snapshot_name), LogLevel::DEBUG);
    }
    s_to_stop.reset(); // Clear the pointer to ensure no dangling references.
    // send reply to app for submitted command
    manageReply(0, STRING_FORMAT("Snapshot '%1%' has been stopped", snapsthot_stop_command->snapshot_name), snapsthot_stop_command);
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
    std::set<std::string> pv_names;
    for (auto& event : *event_received->event_data)
    {
        const std::string pv_name = event->channel_data.pv_name;
        pv_names.insert(pv_name);
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
    // logger->logMessage(STRING_FORMAT("EPICS forwarded %1% data events for PVs: %2%",
    // event_received->event_data->size() % get_pv_names(pv_names)), LogLevel::DEBUG);
}

void ContinuousSnapshotManager::expirationCheckerLoop()
{
    // A list to hold tasks to be submitted outside the lock
    std::vector<std::function<void()>> tasks_to_submit;
    tasks_to_submit.reserve(repeating_snapshot_configuration.snapshot_processing_thread_count);

    while (expiration_thread_running)
    {
        auto now = std::chrono::steady_clock::now();
        {
            std::unique_lock lock(snapshot_runinnig_mutex_);
            for (auto it = snapshot_runinnig_.begin(); it != snapshot_runinnig_.end();)
            {
                auto& queue_name = it->first;
                auto  s_op_ptr = it->second;

                // A snapshot needs processing if its timer has expired OR it has been manually stopped.
                if (s_op_ptr && (s_op_ptr->isTimeout(now)))
                {
                    // --- CAPTURE DATA BEFORE ERASE ---
                    std::string queue_name_copy = queue_name;
                    auto        submission_shard_ptr = s_op_ptr->getData();
                    bool        to_continue = false;
                    bool        last_submission = false;
                    // If the snapshot is not running, it must be cleaned up.
                    if (!s_op_ptr->is_running)
                    {
                        logger->logMessage(STRING_FORMAT("Snapshot %1% is stopped and will be removed from queue", queue_name_copy), LogLevel::INFO);

                        // Remove from pv_snapshot_map_
                        for (auto& pv_uri : s_op_ptr->cmd->pv_name_list)
                        {
                            auto s_pv = epics_service_manager->sanitizePVName(pv_uri);
                            auto range = pv_snapshot_map_.equal_range(s_pv->name);
                            for (auto pv_it = range.first; pv_it != range.second; ++pv_it)
                            {
                                if (pv_it->second == s_op_ptr)
                                {
                                    pv_snapshot_map_.erase(pv_it);
                                    break;
                                }
                            }
                            epics_service_manager->monitorChannel(pv_uri, false);
                        }
                        // Now erase the snapshot and advance the iterator correctly.
                        it = snapshot_runinnig_.erase(it);
                        logger->logMessage(STRING_FORMAT("Snapshot %1% is cancelled", queue_name_copy), LogLevel::INFO);
                        last_submission = to_continue = true; // exit this iteration, do not increment iterator
                    }
                    // also if the snapshot has been removed forward the last data and close the snapshot to the
                    // client print log with submisison information
                    if ((submission_shard_ptr->submission_type & SnapshotSubmissionType::Data) != SnapshotSubmissionType::None)
                    {
                        metrics.incrementCounter(k2eg::service::metric::INodeControllerMetricCounterType::SnapshotEventCounter,
                                                 submission_shard_ptr->snapshot_events.size());
                    }
                    thread_pool->detach_task(
                        [this, s_op_ptr, submission_shard_ptr, last_submission]() mutable
                        {
                            SnapshotSubmissionTask task(s_op_ptr, submission_shard_ptr, this->publisher, this->logger, *this->iteration_sync_, last_submission);
                            task();
                        });
                    if (to_continue)
                    {
                        continue; // Skip the increment, as we already erased the current item.
                    }
                }

                // Only increment if not erased
                ++it;
            }
        } // The lock on snapshot_runinnig_mutex_ is released here.

        // Sleep OUTSIDE the lock
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
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

#pragma region Submission Task

SnapshotSubmissionTask::SnapshotSubmissionTask(std::shared_ptr<SnapshotOpInfo> snapshot_command_info, SnapshotSubmissionShrdPtr submission_shrd_ptr, IPublisherShrdPtr publisher, ILoggerShrdPtr logger, SnapshotIterationSynchronizer& iteration_sync, bool last_submition)
    : snapshot_command_info(snapshot_command_info), submission_shrd_ptr(submission_shrd_ptr), publisher(std::move(publisher)), logger(std::move(logger)), iteration_sync_(iteration_sync), last_submition(last_submition)
{
}

#define CHRONO_TO_UNIX_INT64(x) (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count())

void SnapshotSubmissionTask::operator()()
{
    if (!snapshot_command_info)
        return;
    const auto thread_index = BS::this_thread::get_index();
    if (!thread_index.has_value())
        return;
    auto    snap_ts = CHRONO_TO_UNIX_INT64(submission_shrd_ptr->snap_time);
    int64_t current_iteration = 0;

    // HEADER: This is the start of a new logical iteration.
    if ((submission_shrd_ptr->submission_type & SnapshotSubmissionType::Header) != SnapshotSubmissionType::None)
    {
        // Acquire the lock and get the new, unique iteration number.
        current_iteration = iteration_sync_.acquireIteration(snapshot_command_info->cmd->snapshot_name);
        // Store it in the shared atomic variable for other tasks to see.
        snapshot_command_info->snapshot_iteration_index.store(current_iteration);
    }
    else
    {
        // DATA or TAIL: This task belongs to an existing iteration. Load its number.
        current_iteration = snapshot_command_info->snapshot_iteration_index.load();
    }

    // This guard ensures that this task is counted, and its completion is always registered.
    TaskGuard task_guard(iteration_sync_, snapshot_command_info->cmd->snapshot_name, current_iteration);

    if ((submission_shrd_ptr->submission_type & SnapshotSubmissionType::Header) != SnapshotSubmissionType::None)
    {
        auto serialized_header_message = serialize(RepeatingSnaptshotHeader{0, snapshot_command_info->cmd->snapshot_name, snap_ts, current_iteration},
                                                   snapshot_command_info->cmd->serialization);
        publisher->pushMessage(MakeReplyPushableMessageUPtr(snapshot_command_info->queue_name, "repeating-snapshot-events",
                                                            snapshot_command_info->cmd->snapshot_name, serialized_header_message),
                               {{"k2eg-ser-type", serialization_to_string(snapshot_command_info->cmd->serialization)}});
        logger->logMessage(STRING_FORMAT("[Header] Snapshot %1% iteration %2% started", snapshot_command_info->cmd->snapshot_name % current_iteration), LogLevel::DEBUG);
    }

    if ((submission_shrd_ptr->submission_type & SnapshotSubmissionType::Data) != SnapshotSubmissionType::None &&
        !submission_shrd_ptr->snapshot_events.empty())
    {
        // Data processing logic is unchanged. It can run concurrently with other data tasks.
        std::set<std::string> pv_names_published;
        for (auto& event : submission_shrd_ptr->snapshot_events)
        {
            pv_names_published.insert(event->channel_data.pv_name);
            auto serialized_message =
                serialize(RepeatingSnaptshotData{1, snap_ts, current_iteration, MakeChannelDataShrdPtr(event->channel_data)},
                          snapshot_command_info->cmd->serialization);
            if (serialized_message)
            {
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
        logger->logMessage(STRING_FORMAT("[Data] Snapshot %1% iteration %2% with %3% events from [n. %4%] - %5% - PVs completed",
                                         snapshot_command_info->cmd->snapshot_name % current_iteration %
                                             submission_shrd_ptr->snapshot_events.size() % pv_names_published.size() % get_pv_names(pv_names_published)),
                           LogLevel::DEBUG);
    }

    if ((submission_shrd_ptr->submission_type & SnapshotSubmissionType::Tail) != SnapshotSubmissionType::None)
    {
        logger->logMessage(STRING_FORMAT("[Tail] Snapshot %1% iteration %2% completed", snapshot_command_info->cmd->snapshot_name % current_iteration), LogLevel::DEBUG);
        auto serialized_completion_message = serialize(RepeatingSnaptshotCompletion{2, 0, "", snapshot_command_info->cmd->snapshot_name, snap_ts, current_iteration},
                                                       snapshot_command_info->cmd->serialization);
        publisher->pushMessage(MakeReplyPushableMessageUPtr(snapshot_command_info->queue_name, "repeating-snapshot-events",
                                                            snapshot_command_info->cmd->snapshot_name, serialized_completion_message),
                               {{"k2eg-ser-type", serialization_to_string(snapshot_command_info->cmd->serialization)}});

        // Mark the tail as processed. The lock will be released by finishTask either now (if this is the last task)
        // or later when the last running Data task calls its TaskGuard destructor.
        iteration_sync_.markTailProcessed(snapshot_command_info->cmd->snapshot_name, current_iteration);

        if (last_submition)
        {
            iteration_sync_.removeSnapshot(snapshot_command_info->cmd->snapshot_name);
            snapshot_command_info->removal_promise.set_value(); // Notify that the snapshot is fully removed.
        }
    }
}

#pragma region snapshot iteration synchronization

// Reserve an iteration number and wait if the previous iteration is still in progress.
int64_t SnapshotIterationSynchronizer::acquireIteration(const std::string& snapshot_name)
{
    std::unique_lock<std::shared_mutex> lock(iteration_mutex_);

    // Ensure the condition variable for this snapshot exists before we try to wait on it.
    if (iteration_cv_.find(snapshot_name) == iteration_cv_.end())
    {
        iteration_cv_.emplace(snapshot_name, std::make_unique<std::condition_variable_any>());
    }

    // Wait until the previous iteration is fully complete.
    iteration_cv_.at(snapshot_name)
        ->wait(lock,
               [&]
               {
                   // operator[] on iteration_in_progress_ is safe because std::atomic<bool>
                   // default-constructs to false. A new snapshot is correctly considered "not in progress".
                   auto res = !iteration_in_progress_[snapshot_name];
                   if (!res)
                   {
                       ServiceResolver<ILogger>::resolve()->logMessage(STRING_FORMAT("Waiting for snapshot '%1%' to finish previous iteration", snapshot_name), LogLevel::DEBUG);
                   }
                   return res;
               });

    // Reserve the new iteration.
    uint64_t iteration_id = ++current_iteration_[snapshot_name];
    iteration_in_progress_[snapshot_name] = true;

    // Create a state tracker for this new iteration.
    iteration_states_[snapshot_name][iteration_id] = std::make_shared<IterationState>();
    return iteration_id;
}

// A task calls this to announce it has started. It increments the active task counter.
void SnapshotIterationSynchronizer::startTask(const std::string& snapshot_name, uint64_t iteration_id)
{
    std::shared_lock<std::shared_mutex> lock(iteration_mutex_);
    if (auto it_state = iteration_states_.find(snapshot_name); it_state != iteration_states_.end())
    {
        if (auto it_id_state = it_state->second.find(iteration_id); it_id_state != it_state->second.end())
        {
            it_id_state->second->active_tasks++;
        }
    }
}

// A task calls this to announce it has finished.
void SnapshotIterationSynchronizer::finishTask(const std::string& snapshot_name, uint64_t iteration_id)
{
    std::shared_lock<std::shared_mutex> lock(iteration_mutex_);
    auto                                it_state = iteration_states_.find(snapshot_name);
    if (it_state == iteration_states_.end())
    {
        // The snapshot was removed while this task was running. Nothing to do.
        return;
    }

    if (auto it_id_state = it_state->second.find(iteration_id); it_id_state != it_state->second.end())
    {
        // Decrement active tasks. If it's the last task AND the tail has been processed, release the lock.
        if (--it_id_state->second->active_tasks == 0 && it_id_state->second->tail_processed.load())
        {
            // releaseLock needs an exclusive lock, so we call it without holding the shared_lock
            lock.unlock();
            releaseLock(snapshot_name, iteration_id);
        }
    }
}

// The Tail task calls this to mark the logical end of the iteration.
void SnapshotIterationSynchronizer::markTailProcessed(const std::string& snapshot_name, uint64_t iteration_id)
{
    std::shared_lock<std::shared_mutex> lock(iteration_mutex_);
    if (auto it_state = iteration_states_.find(snapshot_name); it_state != iteration_states_.end())
    {
        if (auto it_id_state = it_state->second.find(iteration_id); it_id_state != it_state->second.end())
        {
            it_id_state->second->tail_processed = true;
            // The check for active_tasks is handled by finishTask.
            // When the Tail task itself finishes, its TaskGuard will call finishTask,
            // which may trigger the release if it's the last one.
        }
    }
}

// Private helper to release the lock and notify the next waiting iteration.
void SnapshotIterationSynchronizer::releaseLock(const std::string& snapshot_name, uint64_t iteration_id_to_clear)
{
    std::lock_guard<std::shared_mutex> lock(iteration_mutex_); // Needs exclusive lock to modify state

    // Check if the snapshot still exists before trying to modify its state.
    // It might have been removed by a stop command.
    auto it_cv = iteration_cv_.find(snapshot_name);
    if (it_cv == iteration_cv_.end())
    {
        return; // Snapshot has been removed, nothing to release or notify.
    }

    iteration_in_progress_[snapshot_name] = false;
    // Clean up the state for the completed iteration to save memory.
    if (auto it_state = iteration_states_.find(snapshot_name); it_state != iteration_states_.end())
    {
        it_state->second.erase(iteration_id_to_clear);
    }
    // Safely notify using the iterator found earlier.
    it_cv->second->notify_one();
}

// Clean up when a snapshot is removed entirely.
void SnapshotIterationSynchronizer::removeSnapshot(const std::string& snapshot_name)
{
    {
        std::lock_guard<std::shared_mutex> lk(iteration_mutex_);
        if (auto it = iteration_cv_.find(snapshot_name); it != iteration_cv_.end())
            it->second->notify_all(); // wake any waiter
    }

    std::this_thread::yield(); // give woken threads a chance to run

    {
        std::lock_guard<std::shared_mutex> lk(iteration_mutex_);
        current_iteration_.erase(snapshot_name);
        iteration_in_progress_.erase(snapshot_name);
        iteration_cv_.erase(snapshot_name); // now nobody can be waiting on it
        iteration_states_.erase(snapshot_name);
    }
}