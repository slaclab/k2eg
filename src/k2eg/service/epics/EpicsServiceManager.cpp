#include "k2eg/common/MsgpackSerialization.h"
#include <k2eg/common/BS_thread_pool.hpp>
#include <k2eg/common/ThrottlingManager.h>
#include <k2eg/common/utility.h>

#include <k2eg/service/ServiceResolver.h>
#include <k2eg/service/epics/EpicsChannel.h>
#include <k2eg/service/epics/EpicsGetOperation.h>
#include <k2eg/service/epics/EpicsMonitorOperation.h>
#include <k2eg/service/epics/EpicsPutOperation.h>
#include <k2eg/service/epics/EpicsServiceManager.h>

#include <chrono>
#include <cstddef>
#include <execution>
#include <memory>
#include <mutex>
#include <ranges>
#include <regex>
#include <vector>

using namespace k2eg::common;

using namespace k2eg::service;

using namespace k2eg::service::log;
using namespace k2eg::service::scheduler;
using namespace k2eg::service::epics_impl;
using namespace k2eg::service::metric;

#define SELECT_PROVIDER(p) (p.find("pva") == 0 ? *pva_provider : *ca_provider)
#define EPICS_MANAGER_STAT_TASK_NAME "epics-manager-stat-task"
#define EPICS_MANAGER_STAT_TASK_CRON "*/1 * * * * *"

void set_thread_name(const std::size_t idx)
{
    // Use a local variable, not static/global
    std::ostringstream oss;
    oss << "Epics Service Monitor " << idx;
    const std::string name = oss.str();
    BS::this_thread::set_os_thread_name(name);
}

inline auto thread_namer = [](unsigned long idx)
{
    set_thread_name(idx);
};

EpicsServiceManager::EpicsServiceManager(ConstEpicsServiceManagerConfigUPtr config)
    : config(std::move(config))
    , end_processing(false)
    , processing_pool(std::make_shared<BS::light_thread_pool>(this->config->thread_count, thread_namer))
    , metric(ServiceResolver<IMetricService>::resolve()->getEpicsMetric())
{
    logger = ServiceResolver<ILogger>::resolve();
    pva_provider = std::make_unique<pvac::ClientProvider>("pva", epics::pvAccess::ConfigurationBuilder().push_env().build());
    ca_provider = std::make_unique<pvac::ClientProvider>("ca", epics::pvAccess::ConfigurationBuilder().push_env().build());
    auto statistic_task = MakeTaskShrdPtr(EPICS_MANAGER_STAT_TASK_NAME, // name of the task
                                          EPICS_MANAGER_STAT_TASK_CRON, // cron expression
                                          std::bind(&EpicsServiceManager::handleStatistic, this, std::placeholders::_1), // task handler
                                          -1 // start at application boot time
    );
    ServiceResolver<Scheduler>::resolve()->addTask(statistic_task);
    logger->logMessage(STRING_FORMAT("[EpicsServiceManager] Epics Service Manager started [tthread_cout=%1%, poll_to=%2%]",
                                     this->config->thread_count % this->config->max_event_from_monitor_queue),
                       LogLevel::INFO);

}

EpicsServiceManager::~EpicsServiceManager()
{
    // stop the statistic
    bool erased = ServiceResolver<Scheduler>::resolve()->removeTaskByName(EPICS_MANAGER_STAT_TASK_NAME);
    // Stop processing monitor tasks and wait for scheduled tasks to finish.
    end_processing = true;
    processing_pool->wait();
    // Clear all registered channels, reset providers and logger
    channel_map.clear();
    pva_provider->reset();
    pva_provider.reset();
    ca_provider->reset();
    ca_provider->reset();
    logger.reset();
}

void EpicsServiceManager::addChannel(const std::string& pv_name_uri)
{
    auto sanitized_pv = sanitizePVName(pv_name_uri);
    if (!sanitized_pv)
        return;

    ChannelTaskShrdPtr new_task;
    {
        WriteLockCM write_lock(channel_map_mutex);
        if (auto it = channel_map.find(sanitized_pv->name); it != channel_map.end())
        {
            auto& existing_task = it->second;
            existing_task->state->keep_alive.fetch_add(1, std::memory_order_relaxed);
            existing_task->state->to_force.store(true, std::memory_order_relaxed);
            return;
        }

        auto new_state = std::make_shared<ChannelMapElement>();
        new_state->channel = std::make_shared<EpicsChannel>(SELECT_PROVIDER(sanitized_pv->protocol), sanitized_pv->name);
        new_state->pv_throttle = std::make_shared<k2eg::common::ThrottlingManager>(
            this->config->pv_min_throttle_us, this->config->pv_max_throttle_us, this->config->pv_idle_threshold);
        new_state->keep_alive.store(1, std::memory_order_relaxed);

        new_task = std::make_shared<ChannelTask>();
        new_task->state = std::move(new_state);
        new_task->monitor = new_task->state->channel->monitor();

        channel_map.emplace(sanitized_pv->name, new_task);
    }
    // Start processing immediately
    processing_pool->detach_task(
        [this, new_task]
        {
            this->task(new_task);
        });
}

StringVector EpicsServiceManager::getMonitoredChannels()
{
    ReadLockCM read_lock(channel_map_mutex);
    auto       kv = channel_map | std::views::transform(
                                [](auto const& kvp) -> const std::string&
                                {
                                    return kvp.first;
                                });

    return k2eg::common::StringVector(kv.begin(), kv.end());
}

void EpicsServiceManager::removeChannel(const std::string& pv_name_uri)
{
    auto sanitized_pv = sanitizePVName(pv_name_uri);
    if (!sanitized_pv)
        return;
    ReadLockCM read_lock(channel_map_mutex);
    if (auto search = channel_map.find(sanitized_pv->name); search != channel_map.end())
    {
        auto& keep_alive = search->second->state->keep_alive;
        int   previous = keep_alive.fetch_sub(1, std::memory_order_relaxed);
        if (previous <= 1)
        {
            keep_alive.store(0, std::memory_order_relaxed);
        }
    }
}

void EpicsServiceManager::monitorChannel(const std::string& pv_identification, bool activate)
{
    if (activate)
    {
        addChannel(pv_identification);
    }
    else
    {
        removeChannel(pv_identification);
    }
}

void EpicsServiceManager::forceMonitorChannelUpdate(const std::string& pv_name_, bool is_uri)
{
    std::string pv_name;
    if (is_uri)
    {
        auto sanitized_pv = sanitizePVName(pv_name_);
        if (!sanitized_pv)
            return;
        pv_name = sanitized_pv->name;
    }
    else
    {
        pv_name = pv_name_;
    }
    ReadLockCM read_lock(channel_map_mutex);
    if (auto search = channel_map.find(pv_name); search != channel_map.end())
    {
        search->second->state->to_force.store(true, std::memory_order_relaxed);
    }
}

ConstMonitorOperationShrdPtr EpicsServiceManager::getMonitorOp(const std::string& pv_name_uri)
{
    auto sanitized_pv = sanitizePVName(pv_name_uri);
    if (!sanitized_pv)
        return ConstMonitorOperationUPtr();

    ReadLockCM read_lock(channel_map_mutex);
    if (auto search = channel_map.find(sanitized_pv->name); search != channel_map.end())
    {
        return search->second->monitor;
    }

    auto channel = std::make_shared<EpicsChannel>(SELECT_PROVIDER(sanitized_pv->protocol), sanitized_pv->name);
    return channel->monitor();
}

ConstGetOperationUPtr EpicsServiceManager::getChannelData(const std::string& pv_name_uri)
{
    ConstGetOperationUPtr result;
    ReadLockCM            read_lock(channel_map_mutex);
    auto                  sanitized_pv = sanitizePVName(pv_name_uri);
    // give a sanitization on pvname, the value will be not used cause k2eg return always all information
    if (!sanitized_pv)
    {
        return ConstGetOperationUPtr();
    }
    if (auto search = channel_map.find(sanitized_pv->name); search != channel_map.end())
    {
        result = search->second->state->channel->get();
    }
    else
    {
        auto channel = std::make_shared<EpicsChannel>(SELECT_PROVIDER(sanitized_pv->protocol), sanitized_pv->name);
        result = channel->get();
    }
    return result;
}

ConstPutOperationUPtr EpicsServiceManager::putChannelData(const std::string& pv_name_uri, std::unique_ptr<MsgpackObject> value)
{
    ConstPutOperationUPtr result;
    ReadLockCM            read_lock(channel_map_mutex);
    auto                  sanitized_pv = sanitizePVName(pv_name_uri);
    if (!sanitized_pv)
    {
        return ConstPutOperationUPtr();
    }
    if (auto search = channel_map.find(sanitized_pv->name); search != channel_map.end())
    {
        result = search->second->state->channel->put(std::move(value));
    }
    else
    {
        auto channel = std::make_shared<EpicsChannel>(SELECT_PROVIDER(sanitized_pv->protocol), sanitized_pv->name);
        result = channel->put(std::move(value));
    }
    return result;
}

size_t EpicsServiceManager::getChannelMonitoredSize()
{
    ReadLockCM read_lock(channel_map_mutex);
    return channel_map.size();
}

k2eg::common::BroadcastToken EpicsServiceManager::addHandler(EpicsServiceManagerHandler new_handler)
{
    std::lock_guard guard(channel_map_mutex);
    return handler_broadcaster.registerHandler(new_handler);
}

size_t EpicsServiceManager::getHandlerSize()
{
    std::lock_guard guard(channel_map_mutex);
    handler_broadcaster.purge();
    return handler_broadcaster.targets.size();
}

// regex for IOC name
std::regex pv_name_regex("^(pva?|ca)://" "([a-zA-Z0-9-_]+(?::[a-zA-Z0-9-_]+)*)(\\.([a-zA-Z0-9-_]+(?:\\.[a-zA-Z0-9_]+)*)" ")?$");

PVUPtr EpicsServiceManager::sanitizePVName(const std::string& pv_name)
{
    std::smatch match;
    std::string protocol;
    std::string base_pv_name;
    std::string field_name = "value";
    // Use std::regex_match to check if pvName matches regExp
    if (!std::regex_match(pv_name, match, pv_name_regex))
    {
        return PVUPtr();
    }

    size_t matches = match.size();
    // Access the groups: match[1] is the PV name without field, match[3] is the field including the '.'
    if (matches > 2)
    {
        protocol = match[1].str();
        base_pv_name = match[2].str();
    }
    // try to decode the multy dot field part
    if (match.size() == 5)
    {
        std::string tmp = match[4].str();
        // Remove the '.' from the start of the field name
        if (!tmp.empty())
        {
            field_name = tmp;
        }
    }
    return std::make_unique<PV>(PV{protocol, base_pv_name, field_name});
}

inline void EpicsServiceManager::recordTaskDuration(const std::chrono::steady_clock::time_point& start_time, const PvRuntimeStatsShrdPtr& pv_stats_ptr)
{
    const auto elapsed_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start_time).count();
    if (pv_stats_ptr)
    {
        pv_stats_ptr->total_duration_ns.fetch_add(static_cast<std::uint64_t>(elapsed_ns), std::memory_order_relaxed);
        pv_stats_ptr->invocation_count.fetch_add(1, std::memory_order_relaxed);
    }
}

inline void EpicsServiceManager::cleanupAndErase(const std::string& pv_name, const ChannelTaskShrdPtr& task_entry, const ChannelMapElementShrdPtr& state, bool erase_from_map, bool record_duration, const std::chrono::steady_clock::time_point& start_time, const PvRuntimeStatsShrdPtr& pv_stats_ptr)
{
    // Flush any pending backlog delta for the counter before resetting stats
    if (pv_stats_ptr)
    {
        const auto delta = pv_stats_ptr->backlog_total_drained.exchange(0, std::memory_order_acq_rel);
        if (delta > 0)
        {
            metric.incrementCounter(IEpicsMetricCounterType::PVBacklogCounter, static_cast<double>(delta), {{"pv", pv_name}});
        }
    }
    metric.incrementCounter(IEpicsMetricCounterType::PVThrottleGauge, 0.0, {{"pv", pv_name}});
    metric.incrementCounter(IEpicsMetricCounterType::PVProcessingDurationGauge, 0.0, {{"pv", pv_name}});
    if (record_duration)
    {
        recordTaskDuration(start_time, pv_stats_ptr);
    }
    if (pv_stats_ptr)
    {
        pv_stats_ptr->total_duration_ns.store(0, std::memory_order_relaxed);
        pv_stats_ptr->invocation_count.store(0, std::memory_order_relaxed);
        pv_stats_ptr->backlog_total_drained.store(0, std::memory_order_relaxed);
    }
    if (state)
    {
        state->active.store(false, std::memory_order_relaxed);
    }
    if (erase_from_map)
    {
        WriteLockCM lock(channel_map_mutex);
        auto        it = channel_map.find(pv_name);
        if (it != channel_map.end() && it->second.get() == task_entry.get())
        {
            channel_map.erase(it);
            logger->logMessage(STRING_FORMAT("Removed PV '%1%' from monitoring", pv_name), LogLevel::INFO);
        }
    }
}

void EpicsServiceManager::task(ChannelTaskShrdPtr task_entry)
{
    if (end_processing || !task_entry)
        return;

    ConstMonitorOperationShrdPtr monitor_op = task_entry->monitor;
    auto                         state = task_entry->state;
    if (!monitor_op || !state)
        return;

    size_t drained = 0;        // number of events drained from the monitor queue
    bool   had_events = false; // true if we had events to process

    const auto  start_time = std::chrono::steady_clock::now(); // start time for duration measurement
    auto        pv_stats_ptr = state->runtime_stats;           // pointer to runtime stats
    const auto& pv_name = monitor_op->getPVName();             // PV name

    if (state->keep_alive.load(std::memory_order_acquire) == 0)
    {
        cleanupAndErase(pv_name, task_entry, state, true, true, start_time, pv_stats_ptr);
        return;
    }

    if (state->to_force.exchange(false, std::memory_order_acq_rel))
    {
        monitor_op->forceUpdate();
    }

    try
    {
        drained = monitor_op->poll(config->max_event_from_monitor_queue);
    }
    catch (const std::exception& ex)
    {
        logger->logMessage(STRING_FORMAT("[EpicsServiceManager::task] poll() failed for PV '%1%': %2%", pv_name % ex.what()), LogLevel::ERROR);
    }
    catch (...)
    {
        logger->logMessage(STRING_FORMAT("[EpicsServiceManager::task] poll() failed for PV '%1%' with unknown error", pv_name), LogLevel::ERROR);
    }

    if (monitor_op->hasEvents())
    {
        EventReceivedShrdPtr received_event;
        try
        {
            received_event = monitor_op->getEventData();
        }
        catch (const std::exception& ex)
        {
            logger->logMessage(STRING_FORMAT("[EpicsServiceManager::task] getEventData() failed for PV '%1%': %2%", pv_name % ex.what()), LogLevel::ERROR);
        }
        catch (...)
        {
            logger->logMessage(STRING_FORMAT("[EpicsServiceManager::task] getEventData() failed for PV '%1%' with unknown error", pv_name), LogLevel::ERROR);
        }

        if (received_event)
        {
            had_events = true;
            const bool is_active = !received_event->event_data->empty() && received_event->event_cancel->empty() &&
                                   received_event->event_disconnect->empty() && received_event->event_fail->empty();
            // set the active state
            state->active.store(is_active, std::memory_order_relaxed);
            //
            handler_broadcaster.broadcast(received_event);
            // update the metric
            metric.incrementCounter(IEpicsMetricCounterType::MonitorData, received_event->event_data->size());
            metric.incrementCounter(IEpicsMetricCounterType::MonitorCancel, received_event->event_cancel->size());
            metric.incrementCounter(IEpicsMetricCounterType::MonitorDisconnect, received_event->event_disconnect->size());
            metric.incrementCounter(IEpicsMetricCounterType::MonitorFail, received_event->event_fail->size());
        }
    }

    if (pv_stats_ptr && drained > 0)
    {
        pv_stats_ptr->backlog_total_drained.fetch_add(static_cast<std::uint64_t>(drained), std::memory_order_relaxed);
    }

    recordTaskDuration(start_time, pv_stats_ptr);

    if (!end_processing)
    {
        if (state->pv_throttle)
        {
            // Use drained event count to adaptively throttle per PV
            state->pv_throttle->update(static_cast<int>(drained));
        }
        processing_pool->detach_task(
            [this, task_entry]
            {
                this->task(task_entry);
            });
    }
}

void EpicsServiceManager::handleStatistic(TaskProperties& task_properties)
{
    int active_pv = 0;
    int total_pv = 0;

    // Snapshot per-PV throttle and backlog to emit outside locks
    struct PvMetricSnapshot
    {
        std::string pv;
        int         throttle_us;
        double      processing_us;
    };

    std::vector<PvMetricSnapshot> pv_metrics;
    {
        ReadLockCM read_lock(channel_map_mutex);
        active_pv = std::count_if(std::execution::par, channel_map.begin(), channel_map.end(),
                                  [](const auto& pair)
                                  {
                                      const auto& state = pair.second->state;
                                      return state && state->active.load(std::memory_order_relaxed);
                                  });
        total_pv = channel_map.size();
        pv_metrics.reserve(channel_map.size());
        for (const auto& [pv_name, task_ptr] : channel_map)
        {
            auto elem = task_ptr->state;
            if (!elem)
                continue;
            int    throttle_us = 0;
            double processing_us = 0.0;
            if (elem->pv_throttle)
            {
                auto t_stat = elem->pv_throttle->getStats();
                throttle_us = t_stat.throttle_us;
            }
            if (elem->runtime_stats)
            {
                const auto total_ns = elem->runtime_stats->total_duration_ns.load(std::memory_order_relaxed);
                const auto invocations = elem->runtime_stats->invocation_count.load(std::memory_order_relaxed);
                if (invocations > 0)
                {
                    processing_us = static_cast<double>(total_ns) / static_cast<double>(invocations) / 1000.0;
                }
                // Flush backlog delta to counter for this PV (accumulates since last statistic)
                const auto delta = elem->runtime_stats->backlog_total_drained.exchange(0, std::memory_order_acq_rel);
                if (delta > 0)
                {
                    metric.incrementCounter(IEpicsMetricCounterType::PVBacklogCounter, static_cast<double>(delta), {{"pv", pv_name}});
                }
            }
            pv_metrics.push_back(PvMetricSnapshot{pv_name, throttle_us, processing_us});
        }
    }
    metric.incrementCounter(IEpicsMetricCounterType::ActiveMonitorGauge, active_pv);
    metric.incrementCounter(IEpicsMetricCounterType::TotalMonitorGauge, total_pv);
    // Emit per-PV metrics for throttle and backlog
    for (const auto& m : pv_metrics)
    {
        metric.incrementCounter(IEpicsMetricCounterType::PVThrottleGauge, m.throttle_us, {{"pv", m.pv}});
        metric.incrementCounter(IEpicsMetricCounterType::PVProcessingDurationGauge, m.processing_us, {{"pv", m.pv}});
    }

    logger->logMessage(STRING_FORMAT("EPICS Monitor Statistics: Active PVs: %1%, Total PVs: %2%", active_pv % total_pv), LogLevel::TRACE);
}

 
