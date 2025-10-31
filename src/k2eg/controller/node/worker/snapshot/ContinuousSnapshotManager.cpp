

#include <k2eg/common/BaseSerialization.h>
#include <k2eg/common/utility.h>

#include <k2eg/service/ServiceResolver.h>
#include <k2eg/service/epics/EpicsData.h>
#include <k2eg/service/metric/INodeControllerMetric.h>

#include <k2eg/controller/command/cmd/SnapshotCommand.h>
#include <k2eg/controller/node/worker/SnapshotCommandWorker.h>
#include <k2eg/controller/node/worker/snapshot/BackTimedBufferedSnapshotOpInfo.h>
#include <k2eg/controller/node/worker/snapshot/ContinuousSnapshotManager.h>
#include <k2eg/controller/node/worker/snapshot/SnapshotOpInfo.h>
#include <k2eg/controller/node/worker/snapshot/SnapshotRepeatingOpInfo.h>

#include <boost/json.hpp>

#include <algorithm>
#include <chrono>
#include <memory>
#include <mutex>
#include <regex>
#include <set>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <utility>

using namespace k2eg::controller::command;
using namespace k2eg::controller::command::cmd;
using namespace k2eg::controller::node::worker;
using namespace k2eg::controller::node::worker::snapshot;

using namespace k2eg::service;
using namespace k2eg::service::log;
using namespace k2eg::service::pubsub;
using namespace k2eg::service::metric;
using namespace k2eg::service::scheduler;
using namespace k2eg::service::epics_impl;
using namespace k2eg::service::configuration;

#define CSM_RECURRING_CHECK_TASK_NAME       "csm-recurring-check-task"
#define CSM_RECURRING_CHECK_TASK_CRON       "1 * * * * *"

#pragma region Utility
#define GET_QUEUE_FROM_SNAPSHOT_NAME(snapshot_name) ([](const std::string& name) { \
    std::string norm = std::regex_replace(name, std::regex(R"([^A-Za-z0-9\-])"), "_"); \
    std::transform(norm.begin(), norm.end(), norm.begin(), ::tolower); \
    return norm; \
})(snapshot_name)

const std::string now_in_gmt()
{
    auto        now = std::chrono::system_clock::now();
    std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
    char        timestamp_buf[32];
    std::strftime(timestamp_buf, sizeof(timestamp_buf), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now_time_t));
    return timestamp_buf;
}

// Local aggregator to count events per snapshot iteration and log at Tail.
namespace {
std::mutex                                g_tail_log_mutex;
std::unordered_map<std::string, uint64_t> g_tail_event_counters;

inline std::string make_tail_key(const std::string& snapshot_name, int64_t iteration)
{
    return snapshot_name + "#" + std::to_string(iteration);
}

inline void add_events_for_iteration(const std::string& snapshot_name, int64_t iteration, uint64_t count)
{
    if (count == 0)
        return;
    std::lock_guard<std::mutex> lk(g_tail_log_mutex);
    g_tail_event_counters[make_tail_key(snapshot_name, iteration)] += count;
}

inline uint64_t take_events_for_iteration(const std::string& snapshot_name, int64_t iteration)
{
    std::lock_guard<std::mutex> lk(g_tail_log_mutex);
    auto                        key = make_tail_key(snapshot_name, iteration);
    auto                        it = g_tail_event_counters.find(key);
    if (it == g_tail_event_counters.end())
        return 0;
    auto n = it->second;
    g_tail_event_counters.erase(it);
    return n;
}
} // namespace

void set_snapshot_thread_name(const char* name, const std::size_t idx)
{
    // Use a local variable, not static/global
    std::ostringstream oss;
    oss << name << idx;
    const std::string name_ = oss.str();
    BS::this_thread::set_os_thread_name(name_);
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

inline auto thread_namer_submission = [](unsigned long idx)
{
    set_snapshot_thread_name("Repeating Snapshot Submitting", idx);
};

inline auto thread_namer_daq_processing = [](unsigned long idx)
{
    set_snapshot_thread_name("Repeating Snapshot DAQ Processing", idx);
};

#pragma region Implementation

ContinuousSnapshotManager::ContinuousSnapshotManager(const RepeatingSnapshotConfiguration& repeating_snapshot_configuration, k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager)
    : repeating_snapshot_configuration(repeating_snapshot_configuration)
    , logger(ServiceResolver<ILogger>::resolve())
    , node_configuration(ServiceResolver<service::configuration::INodeConfiguration>::resolve())
    , publisher(ServiceResolver<IPublisher>::resolve())
    , epics_service_manager(epics_service_manager)
    , thread_pool_submitting(std::make_shared<BS::light_thread_pool>(repeating_snapshot_configuration.snapshot_processing_thread_count, thread_namer_submission))
    , thread_pool_daq_processing(std::make_shared<BS::light_thread_pool>(1, thread_namer_daq_processing))
    , metrics(ServiceResolver<IMetricService>::resolve()->getNodeControllerMetric())
    , iteration_sync_(std::make_unique<SnapshotIterationSynchronizer>())
{
    logger->logMessage("Initializing continuous snapshot manager", LogLevel::INFO);
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
    logger->logMessage(
        STRING_FORMAT("Continuous snapshot manager initialized (threads=%1%)",
                       repeating_snapshot_configuration.snapshot_processing_thread_count),
        LogLevel::INFO);

    auto task_restart_monitor = MakeTaskShrdPtr(CSM_RECURRING_CHECK_TASK_NAME, CSM_RECURRING_CHECK_TASK_CRON, std::bind(&ContinuousSnapshotManager::handlePeriodicTask, this, std::placeholders::_1),
                                                -1 // start at application boot time
    );
    ServiceResolver<Scheduler>::resolve()->addTask(task_restart_monitor);
}

ContinuousSnapshotManager::~ContinuousSnapshotManager()
{
    logger->logMessage("Stopping continuous snapshot manager");

    // remove the periodic task
    logger->logMessage("Remove periodic task from scheduler");
    bool erased = ServiceResolver<Scheduler>::resolve()->removeTaskByName(CSM_RECURRING_CHECK_TASK_NAME);
    logger->logMessage(STRING_FORMAT("Removed periodic maintanance : %1%", erased));

    // stop the expiration thread
    logger->logMessage("Stopping expiration thread");
    expiration_thread_running = false;
    // Wake any threads waiting on iteration condition variables to allow clean shutdown
    if (iteration_sync_)
    {
        iteration_sync_->notifyAll();
    }
    if (expiration_thread.joinable())
    {
        expiration_thread.join();
    }
    logger->logMessage("Expiration thread stopped");
    // set the run flag to false
    run_flag = false;
    // remove epics monitor handler
    epics_handler_token.reset();
    // stop all the thread
    thread_pool_daq_processing->wait();
    thread_pool_submitting->wait();
    logger->logMessage("[ContinuousSnapshotManager] Continuous snapshot manager stopped");
}

std::size_t ContinuousSnapshotManager::getRunningSnapshotCount() const
{
    std::unique_lock write_lock(snapshot_running_mutex_);
    // check if the snapshot is already stopped
    return snapshot_running_.size();
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
    logger->logMessage(STRING_FORMAT("Prepare continuous (triggered %4%) snapshot ops for '%1%' on topic %2% with ser-type: %3%",
                                     snapsthot_command->snapshot_name % snapsthot_command->reply_topic %
                                         serialization_to_string(snapsthot_command->serialization) % snapsthot_command->triggered),
                       LogLevel::DEBUG);
    // create the command operation info structure
    SnapshotOpInfoShrdPtr s_op_ptr = nullptr;
    switch (snapsthot_command->snapshot_type)
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
        std::unique_lock write_lock(snapshot_running_mutex_);
        if (snapshot_running_.find(s_op_ptr->queue_name) != snapshot_running_.end())
        {
            manageReply(0, STRING_FORMAT("Snapshot %1% is already running", s_op_ptr->cmd->snapshot_name), snapsthot_command);
            return;
        }
    }

    // check if all the command information are filled
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

    // try to gain the ownership of the snapshot
    if (!tryToConfigureSnapshotStart(*s_op_ptr))
    {
        manageReply(-8, STRING_FORMAT("The snapshot %1% is already running by %2%", s_op_ptr->cmd->snapshot_name % node_configuration->getSnapshotGateway(s_op_ptr->queue_name)), snapsthot_command);
        return;
    }

    for (const auto& pv_uri : s_op_ptr->cmd->pv_name_list)
    {
        // get pv name sanitization
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
        std::unique_lock write_lock(snapshot_running_mutex_);
        // associate the topic to the snapshot in the running list
        snapshot_running_.emplace(s_op_ptr->queue_name, s_op_ptr);
        for (auto& pv_uri : sanitized_pv_name_list)
        {
            logger->logMessage(
                STRING_FORMAT("associate snapshot '%1%' to pv '%2%'", s_op_ptr->cmd->snapshot_name % pv_uri->name), LogLevel::DEBUG);
            // associate snapshot to each pv, so the epics handler can
            // find the snapshot to fill with data
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
        // queue name, the normalized version of the snapshot name is used as key to stop the snapshot
        auto queue_name = GET_QUEUE_FROM_SNAPSHOT_NAME(snapshot_trigger_command->snapshot_name);
        // acquire the write lock to stop the snapshot
        std::unique_lock write_lock(snapshot_running_mutex_);
        // check if the snapshot is already stopped
        auto it = snapshot_running_.find(queue_name);
        if (it != snapshot_running_.end())
        {
            // set snapshot as to stop
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
    // queue name, the normalized version of the snapshot name is used as key to stop the snapshot
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
        std::shared_lock write_lock(snapshot_running_mutex_);
        // check if the snapshot is already stopped
        auto it = snapshot_running_.find(queue_name);
        if (it != snapshot_running_.end())
        {
            // Get the future before we mark it for stopping
            s_to_stop = it->second;
            removal_future = s_to_stop->removal_promise.get_future();
            // set snapshot as to stop
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
    // Now we can safely remove the snapshot from the running list
    releaseSnapshotForStop(*s_to_stop);
    s_to_stop.reset(); // Clear the pointer to ensure no dangling references.

    // send reply to app for submitted command
    manageReply(0, STRING_FORMAT("Snapshot '%1%' has been stopped", snapsthot_stop_command->snapshot_name), snapsthot_stop_command);
}

void ContinuousSnapshotManager::epicsMonitorEvent(EpicsServiceManagerHandlerParamterType event_received)
{
    // process the received event in a separate thread to not block the epics thread
    thread_pool_daq_processing->detach_task(
        [this, event_received]() mutable
        {
            // prepare reading lock
            std::shared_lock read_lock(snapshot_running_mutex_);
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
        });
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
            std::unique_lock lock(snapshot_running_mutex_);
            for (auto it = snapshot_running_.begin(); it != snapshot_running_.end();)
            {
                auto& queue_name = it->first;
                auto  s_op_ptr = it->second;

                // A snapshot needs processing if its timer has expired OR it has been manually stopped.
                if (s_op_ptr && (s_op_ptr->isTimeout(now)))
                {
                    // --- CAPTURE DATA BEFORE ERASE ---
                    std::string queue_name_copy = queue_name;
                    auto        submission_shard_ptr = s_op_ptr->getData();
                    auto        non_updated_pvs = s_op_ptr->getPVsWithoutEvents();
                    bool        to_continue = false;
                    bool        last_submission = false;

                    for (const auto& pv_name : non_updated_pvs)
                    {
                        // Forward the last data for each non-updated PV
                        epics_service_manager->forceMonitorChannelUpdate(pv_name, false);
                    }

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
                        it = snapshot_running_.erase(it);
                        logger->logMessage(STRING_FORMAT("Snapshot %1% is cancelled", queue_name_copy), LogLevel::INFO);
                        last_submission = to_continue = true; // exit this iteration, do not increment iterator
                    }
                    // also if the snapshot has been removed forward the last data and close the snapshot to the
                    // client print log with submission information
                    if ((submission_shard_ptr->submission_type & SnapshotSubmissionType::Data) != SnapshotSubmissionType::None)
                    {
                        metrics.incrementCounter(k2eg::service::metric::INodeControllerMetricCounterType::SnapshotEventCounter,
                                                 submission_shard_ptr->snapshot_events.size());
                    }
                    // Determine iteration id and set per-iteration gates/counters
                    int64_t scheduled_iteration_id = 0;
                    if ((submission_shard_ptr->submission_type & SnapshotSubmissionType::Header) != SnapshotSubmissionType::None)
                    {
                        scheduled_iteration_id = iteration_sync_->acquireIteration(s_op_ptr->cmd->snapshot_name);
                        active_iteration_id_[s_op_ptr->cmd->snapshot_name] = scheduled_iteration_id;
                        s_op_ptr->beginHeaderGate(scheduled_iteration_id);
                    }
                    else
                    {
                        auto it = active_iteration_id_.find(s_op_ptr->cmd->snapshot_name);
                        if (it != active_iteration_id_.end())
                            scheduled_iteration_id = it->second;
                        else
                            scheduled_iteration_id = 0; // should not happen if scheduling order is correct
                    }

                    // Always attach resolved iteration id to the submission for task-side use
                    submission_shard_ptr->iteration_id = scheduled_iteration_id;

                    // If Data is present, record it so Tail can wait later. Count per submission batch.
                    if ((submission_shard_ptr->submission_type & SnapshotSubmissionType::Data) != SnapshotSubmissionType::None &&
                        !submission_shard_ptr->snapshot_events.empty())
                    {
                        s_op_ptr->dataScheduled(scheduled_iteration_id);
                    }
                    thread_pool_submitting->detach_task(
                        [this, s_op_ptr, submission_shard_ptr, last_submission]() mutable
                        {
                            SnapshotSubmissionTask task(s_op_ptr, submission_shard_ptr, this->publisher, this->logger, *this->iteration_sync_, last_submission);
                            task();
                        });
                    if (to_continue)
                    {
                        // Cleanup active iteration tracking for this snapshot
                        active_iteration_id_.erase(s_op_ptr->cmd->snapshot_name);
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

void ContinuousSnapshotManager::handlePeriodicTask(TaskProperties& task_properties)
{
    logger->logMessage("Handle periodic task for recurring snapshot management", LogLevel::DEBUG);
    // check for snapshot to reboot using configuration service
    if (!node_configuration)
    {
        logger->logMessage("Node configuration service not available", LogLevel::ERROR);
        return;
    }
    // get the list of snapshots to be restarted
    auto snapshots_to_restart = node_configuration->getAvailableSnapshot();
    if (snapshots_to_restart.empty())
    {
        logger->logMessage("No snapshots available for restart", LogLevel::DEBUG);
        return;
    }

    logger->logMessage(STRING_FORMAT("Restart first of %1% snapshots to restart", snapshots_to_restart.size()), LogLevel::DEBUG);
    auto snapshot_id = snapshots_to_restart.front();
    auto snapshot_config = node_configuration->getSnapshotConfiguration(snapshot_id);
    if (!snapshot_config)
    {
        logger->logMessage(STRING_FORMAT("Snapshot configuration for '%1%' not found", snapshot_id), LogLevel::ERROR);
        return;
    }
    // Submit the command to start the snapshot
    logger->logMessage(STRING_FORMAT("Submitting repeating snapshot command for '%1%' with cmd: %2%", snapshot_id % snapshot_config->config_json), LogLevel::DEBUG);
    // create empty command
    auto snapshot_command = MakeRepeatingSnapshotCommandShrdPtr(RepeatingSnapshotCommand{});
    // create boost json object from snapshot configuration
    from_json(snapshot_config->config_json, *snapshot_command);
    // on auto-restart we need to remove away the reply id and topic
    // ok we can submit the command
    submitSnapshot(snapshot_command);
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

#pragma region Configuration

bool ContinuousSnapshotManager::tryToConfigureSnapshotStart(SnapshotOpInfo& snapshot_ops_info)
{
    // This method should attempt to configure the snapshot start using the node configuration.
    // Return true if configuration was successful, false otherwise.
    bool result = false;
    try
    {
        logger->logMessage(STRING_FORMAT("Attempting to acquire ownership of snapshot '%1%'", snapshot_ops_info.queue_name), LogLevel::INFO);
        // Attempt to configure using the node_configuration interface.
        // The actual implementation depends on your INodeConfiguration interface.
        // Here, we assume it returns a bool indicating success.
        if (!node_configuration)
        {
            logger->logMessage("Node configuration service not available", LogLevel::ERROR);
            return result;
        }

        result = node_configuration->tryAcquireSnapshot(snapshot_ops_info.queue_name, true);
        if (!result)
        {
            // acquistion has not been successful
            logger->logMessage(STRING_FORMAT("Snapshot '%1%' cannot be acquired by '%2%'", snapshot_ops_info.queue_name % node_configuration->getNodeName()), LogLevel::INFO);
            return result;
        }

        // If the snapshot is acquired, we can proceed to configure it.
        logger->logMessage(STRING_FORMAT("Snapshot '%1%' acquired by '%2%'", snapshot_ops_info.queue_name % node_configuration->getNodeName()), LogLevel::INFO);

        // create json description for ConstRepeatingSnapshotCommandShrdPtr
        snapshot_ops_info.snapshot_configuration = MakeSnapshotConfigurationShrdPtr(
            SnapshotConfiguration{
                .weight = 0,
                .weight_unit = "eps",
                .update_timestamp = now_in_gmt(),
                .config_json = to_json_string_cmd_ptr(snapshot_ops_info.cmd)});
        result = node_configuration->setSnapshotConfiguration(snapshot_ops_info.queue_name, snapshot_ops_info.snapshot_configuration);
        // set the snapshot as running
        node_configuration->setSnapshotRunning(snapshot_ops_info.queue_name, true);
        node_configuration->setSnapshotArchiveRequested(snapshot_ops_info.queue_name, true);
    }
    catch (const std::exception& ex)
    {
        logger->logMessage(
            STRING_FORMAT("Exception during tryToConfigureSnapshotStart for '%1%': %2%", snapshot_ops_info.queue_name % ex.what()),
            LogLevel::ERROR);
    }
    return result;
}

bool ContinuousSnapshotManager::releaseSnapshotForStop(SnapshotOpInfo& snapshot_ops_info)
{
    if (!node_configuration)
    {
        logger->logMessage("Node configuration service not available", LogLevel::ERROR);
        return false;
    }
    logger->logMessage(STRING_FORMAT("Setting snapshot '%1%' running status to %2%", snapshot_ops_info.queue_name % false), LogLevel::INFO);
    // Update the snapshot's running status in the node configuration.
    node_configuration->setSnapshotRunning(snapshot_ops_info.queue_name, false);

    // release the snapshot ownership
    if (!node_configuration->releaseSnapshot(snapshot_ops_info.queue_name, true))
    {
        logger->logMessage(STRING_FORMAT("Failed to release snapshot '%1%'", snapshot_ops_info.queue_name), LogLevel::ERROR);
    }
    return true;
}

#pragma region Submission Task

SnapshotSubmissionTask::SnapshotSubmissionTask(std::shared_ptr<SnapshotOpInfo> snapshot_command_info, SnapshotSubmissionShrdPtr submission_shrd_ptr, IPublisherShrdPtr publisher, ILoggerShrdPtr logger, SnapshotIterationSynchronizer& iteration_sync, bool last_submission)
    : snapshot_command_info(snapshot_command_info), submission_shrd_ptr(submission_shrd_ptr), publisher(std::move(publisher)), logger(std::move(logger)), iteration_sync_(iteration_sync), last_submission(last_submission)
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
    auto    header_timestamp = CHRONO_TO_UNIX_INT64(submission_shrd_ptr->header_timestamp);
    int64_t current_iteration = submission_shrd_ptr->iteration_id;
    auto    statistic_counter = snapshot_command_info->getStatisticCounter();
    // Iteration id is resolved at scheduling and attached to the submission.
    if (current_iteration == 0)
    {
        logger->logMessage(
            STRING_FORMAT("Snapshot %1% missing iteration id on submission; skipping.", snapshot_command_info->cmd->snapshot_name), LogLevel::ERROR);
        return;
    }

    // This guard ensures that this task is counted, and its completion is always registered.
    TaskGuard task_guard(iteration_sync_, snapshot_command_info->cmd->snapshot_name, current_iteration);

    // HEADER: This is the start of a new logical iteration.
    if ((submission_shrd_ptr->submission_type & SnapshotSubmissionType::Header) != SnapshotSubmissionType::None)
    {
        snapshot_command_info->publishHeader(publisher, logger, snap_ts, current_iteration);
        logger->logMessage(STRING_FORMAT("[Header] Snapshot %1% iteration %2% started", snapshot_command_info->cmd->snapshot_name % current_iteration), LogLevel::DEBUG);

        // Release header gate so Data submissions can proceed
        snapshot_command_info->completeHeaderGate(current_iteration);
    }

    if ((submission_shrd_ptr->submission_type & SnapshotSubmissionType::Data) != SnapshotSubmissionType::None)
    {
        // Ensure header was published for this iteration before sending any data
        snapshot_command_info->waitForHeaderGate(current_iteration);

        // Publish data via SnapshotOpInfo and aggregate counts for Tail logging
        const auto events_sent_this_batch =
            snapshot_command_info->publishData(publisher, logger, snap_ts, header_timestamp, current_iteration, submission_shrd_ptr->snapshot_events);
        add_events_for_iteration(snapshot_command_info->cmd->snapshot_name, current_iteration, events_sent_this_batch);
        // Mark data submission as completed for this iteration
        snapshot_command_info->dataCompleted(current_iteration);
    }

    if ((submission_shrd_ptr->submission_type & SnapshotSubmissionType::Tail) != SnapshotSubmissionType::None)
    {
        // Ensure all data submissions for this iteration are fully published before sending Tail
        snapshot_command_info->waitDataDrained(current_iteration);
        // Emit a single tail log with aggregated event count for this iteration
        const auto events_published = take_events_for_iteration(snapshot_command_info->cmd->snapshot_name, current_iteration);
        logger->logMessage(
            STRING_FORMAT("[Tail] Snapshot %1% iteration %2% completed, events=%3%", snapshot_command_info->cmd->snapshot_name % current_iteration % events_published), LogLevel::DEBUG);
        snapshot_command_info->publishTail(publisher, logger, snap_ts,  header_timestamp, current_iteration);

        // Mark the tail as processed. The lock will be released by finishTask either now (if this is the last task)
        // or later when the last running Data task calls its TaskGuard destructor.
        iteration_sync_.markTailProcessed(snapshot_command_info->cmd->snapshot_name, current_iteration);

        if (last_submission)
        {
            iteration_sync_.removeSnapshot(snapshot_command_info->cmd->snapshot_name);
            snapshot_command_info->removal_promise.set_value(); // Notify that the snapshot is fully removed.
        }
    }

    // No explicit header gate clear is needed; gate resets on the next Header scheduling
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

// dataStarted/dataCompleted/waitDataDrained now managed inside SnapshotOpInfo

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

void SnapshotIterationSynchronizer::notifyAll()
{
    std::lock_guard<std::shared_mutex> lk(iteration_mutex_);
    for (auto &kv : iteration_cv_)
    {
        if (kv.second)
        {
            kv.second->notify_all();
        }
    }
}
