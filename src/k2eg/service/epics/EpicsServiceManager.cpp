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

#include <cstddef>
#include <execution>
#include <memory>
#include <vector>
#include <mutex>
#include <ranges>
#include <regex>

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
    , thread_throttling_vector(this->config->thread_count)
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

    bool inserted = false;
    try
    {
        std::shared_ptr<EpicsChannel> channel_ptr;
        {
            WriteLockCM write_lock(channel_map_mutex);
            auto [it, success] = channel_map.emplace(
                sanitized_pv->name,
                ChannelMapElement{
                    .channel = std::make_shared<EpicsChannel>(SELECT_PROVIDER(sanitized_pv->protocol), sanitized_pv->name),
                    .to_force = false,
                    .keep_alive = 1,
                    .active = false,
                    .pv_throttle = std::make_shared<k2eg::common::ThrottlingManager>(
                        this->config->pv_min_throttle_us, this->config->pv_max_throttle_us, this->config->pv_idle_threshold),
                });
            if (!success)
            {
                // Already exists, increment keep_alive and set to_force
                it->second.keep_alive++;
                it->second.to_force = true;
                return;
            }
            channel_ptr = it->second.channel;
            inserted = true;
        }

        // Only monitor if we actually inserted a new channel
        ConstMonitorOperationShrdPtr monitor_operation = channel_ptr->monitor();
        processing_pool->detach_task(
            [this, monitor_operation]
            {
                this->task(monitor_operation);
            });
    }
    catch (...)
    {
        // Only erase if we actually inserted
        if (inserted)
        {
            WriteLockCM write_lock(channel_map_mutex);
            channel_map.erase(sanitized_pv->name);
        }
        throw;
    }
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
        search->second.keep_alive--;
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
        search->second.to_force = true;
    }
}

ConstMonitorOperationShrdPtr EpicsServiceManager::getMonitorOp(const std::string& pv_name_uri)
{
    auto sanitized_pv = sanitizePVName(pv_name_uri);
    if (!sanitized_pv)
        return ConstMonitorOperationUPtr();

    ConstMonitorOperationShrdPtr result;
    ReadLockCM                   read_lock(channel_map_mutex);
    if (auto search = channel_map.find(sanitized_pv->name); search != channel_map.end())
    {
        result = channel_map[sanitized_pv->name].channel->monitor();
    }
    else
    {
        auto channel = ChannelMapElement{
            .channel = std::make_shared<EpicsChannel>(SELECT_PROVIDER(sanitized_pv->protocol), sanitized_pv->name),
            .to_force = false,
            .keep_alive = 0,
            .active = false,
            .pv_throttle = std::make_shared<k2eg::common::ThrottlingManager>(
                this->config->pv_min_throttle_us, this->config->pv_max_throttle_us, this->config->pv_idle_threshold),
        };
        result = channel.channel->monitor();
    }
    return result;
}

ConstGetOperationUPtr EpicsServiceManager::getChannelData(const std::string& pv_name_uri)
{
    ConstGetOperationUPtr result;
    WriteLockCM           write_lock(channel_map_mutex);
    auto                  sanitized_pv = sanitizePVName(pv_name_uri);
    // give a sanitization on pvname, the value will be not used cause k2eg return always all information
    if (!sanitized_pv)
    {
        return ConstGetOperationUPtr();
    }
    if (auto search = channel_map.find(sanitized_pv->name); search != channel_map.end())
    {
        // allocate channel and return data
        result = channel_map[sanitized_pv->name].channel->get();
    }
    else
    {
        auto channel = ChannelMapElement{
            .channel = std::make_shared<EpicsChannel>(SELECT_PROVIDER(sanitized_pv->protocol), sanitized_pv->name),
            .to_force = false,
            .keep_alive = 0,
            .active = false,
            .pv_throttle = std::make_shared<k2eg::common::ThrottlingManager>(
                this->config->pv_min_throttle_us, this->config->pv_max_throttle_us, this->config->pv_idle_threshold),
        };
        result = channel.channel->get();
    }
    return result;
}

ConstPutOperationUPtr EpicsServiceManager::putChannelData(const std::string& pv_name_uri, std::unique_ptr<MsgpackObject> value)
{
    ConstPutOperationUPtr result;
    WriteLockCM           write_lock(channel_map_mutex);
    auto                  sanitized_pv = sanitizePVName(pv_name_uri);
    if (!sanitized_pv)
    {
        return ConstPutOperationUPtr();
    }
    if (auto search = channel_map.find(sanitized_pv->name); search != channel_map.end())
    {
        // allocate channel and return data
        result = channel_map[sanitized_pv->name].channel->put(std::move(value));
    }
    else
    {
        auto channel = ChannelMapElement{
            .channel = std::make_shared<EpicsChannel>(SELECT_PROVIDER(sanitized_pv->protocol), sanitized_pv->name),
            .to_force = false,
            .keep_alive = 0,
        };
        result = channel.channel->put(std::move(value));
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

const std::vector<ThrottlingManager>& EpicsServiceManager::getThreadThrottlingInfo() const
{
    return thread_throttling_vector;
}

void EpicsServiceManager::task(ConstMonitorOperationShrdPtr monitor_op)
{
    if (end_processing)
        return;
    const auto thread_index = BS::this_thread::get_index();
    if (!thread_index.has_value())
        return;

    bool        had_events = false;
    bool        local_to_force = false;
    bool        should_delete = false;
    const auto& pv_name = monitor_op->getPVName();
    // keep per-thread throttling around for metrics, but we will prefer per-PV throttling below
    auto&                                            throttling = thread_throttling_vector[thread_index.value()];
    std::shared_ptr<k2eg::common::ThrottlingManager> pv_throttle_ptr;

    // Step 1: inspect and possibly update flags under lock
    {
        WriteLockCM lock(channel_map_mutex);
        auto        it = channel_map.find(pv_name);
        if (it == channel_map.end())
        {
            return;
        }
        should_delete = (it->second.keep_alive == 0);
        if (!should_delete)
        {
            local_to_force = it->second.to_force;
            if (local_to_force)
            {
                it->second.to_force = false; // clear under lock; apply force after unlocking
            }
            pv_throttle_ptr = it->second.pv_throttle; // copy shared_ptr under lock
        }
        else
        {
            // remove now under lock
            channel_map.erase(it);
        }
    }

    if (should_delete)
    {
        // Emit zero to clear per-PV gauges when the PV is removed
        metric.incrementCounter(IEpicsMetricCounterType::PVThrottleGauge, 0.0, {{"pv", pv_name}});
        metric.incrementCounter(IEpicsMetricCounterType::PVBacklogGauge, 0.0, {{"pv", pv_name}});
        throttling.reset();
        return;
    }

    if (local_to_force)
    {
        monitor_op->forceUpdate();
    }

    // Step 2: fetch bounded data from monitor (robust to provider exceptions)
    size_t drained = 0;
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

    // Step 3: if there are events, broadcast them
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

            // compute 'active' as: data present AND no errors
            const bool is_active = !received_event->event_data->empty() && received_event->event_cancel->empty() &&
                                   received_event->event_disconnect->empty() && received_event->event_fail->empty();

            // update active flag (and possibly delete) under lock
            bool delete_after_broadcast = false;
            {
                WriteLockCM lock(channel_map_mutex);
                auto        it2 = channel_map.find(pv_name);
                if (it2 != channel_map.end())
                {
                    it2->second.active = is_active;
                    if (it2->second.keep_alive == 0)
                    {
                        channel_map.erase(it2);
                        delete_after_broadcast = true;
                    }
                }
            }

            // metrics and broadcast outside locks
            metric.incrementCounter(IEpicsMetricCounterType::MonitorData, received_event->event_data->size());
            metric.incrementCounter(IEpicsMetricCounterType::MonitorCancel, received_event->event_cancel->size());
            metric.incrementCounter(IEpicsMetricCounterType::MonitorDisconnect, received_event->event_disconnect->size());
            metric.incrementCounter(IEpicsMetricCounterType::MonitorFail, received_event->event_fail->size());

            handler_broadcaster.broadcast(received_event);

            if (delete_after_broadcast)
            {
                // Emit zero to clear per-PV gauges when the PV is removed
                metric.incrementCounter(IEpicsMetricCounterType::PVThrottleGauge, 0.0, {{"pv", pv_name}});
                metric.incrementCounter(IEpicsMetricCounterType::PVBacklogGauge, 0.0, {{"pv", pv_name}});
                throttling.reset();
                return;
            }
        }
    }

    // manage throttling: prefer per-PV throttling to avoid sleeping entire thread due to one idle PV
    if (pv_throttle_ptr)
    {
        // Per-PV backoff in microseconds
        pv_throttle_ptr->update(had_events);
        // Backlog gauge: report number of drained items when backlog is present (hit per-pass cap), else 0.
        const auto   max_per_pass = static_cast<size_t>(config->max_event_from_monitor_queue);
        const double backlog_count = drained >= max_per_pass ? static_cast<double>(drained) : 0.0;
        // Store backlog count for periodic metric emission in handleStatistic().
        {
            WriteLockCM lock(channel_map_mutex);
            auto        it = channel_map.find(pv_name);
            if (it != channel_map.end())
            {
                it->second.last_backlog_count = backlog_count;
            }
        }
    }

    // resubmit the task to the thread pool unless shutting down
    if (!end_processing)
    {
        processing_pool->detach_task(
            [this, monitor_op]
            {
                this->task(monitor_op);
            });
    }
}

void EpicsServiceManager::handleStatistic(TaskProperties& task_properties)
{
    int         active_pv = 0;
    int         total_pv = 0;
    std::string throttling_info;
    // Snapshot per-PV throttle and backlog to emit outside locks
    struct PvMetricSnapshot { std::string pv; int throttle_us; double backlog; };
    std::vector<PvMetricSnapshot> pv_metrics;
    {
        ReadLockCM read_lock(channel_map_mutex);
        active_pv = std::count_if(std::execution::par, channel_map.begin(), channel_map.end(),
                                  [](const auto& pair)
                                  {
                                      return pair.second.active == true;
                                  });
        total_pv = channel_map.size();
        pv_metrics.reserve(channel_map.size());
        for (const auto& [pv_name, elem] : channel_map)
        {
            int    throttle_us = 0;
            double backlog     = elem.last_backlog_count;
            if (elem.pv_throttle)
            {
                auto t_stat = elem.pv_throttle->getStats();
                throttle_us = t_stat.throttle_us;
            }
            pv_metrics.push_back(PvMetricSnapshot{pv_name, throttle_us, backlog});
        }
    }
    metric.incrementCounter(IEpicsMetricCounterType::ActiveMonitorGauge, active_pv);
    metric.incrementCounter(IEpicsMetricCounterType::TotalMonitorGauge, total_pv);
    // Emit per-PV metrics for throttle and backlog
    for (const auto& m : pv_metrics)
    {
        metric.incrementCounter(IEpicsMetricCounterType::PVThrottleGauge, m.throttle_us, {{"pv", m.pv}});
        metric.incrementCounter(IEpicsMetricCounterType::PVBacklogGauge, m.backlog, {{"pv", m.pv}});
    }
    if (!config->disable_thread_throttle)
    {
        auto& thread_throttling_vector = getThreadThrottlingInfo();
        for (std::size_t i = 0; i < thread_throttling_vector.size(); ++i)
        {
            const auto& throttling = thread_throttling_vector[i];
            std::string thread_id = std::to_string(i);
            auto        t_stat = throttling.getStats();
            metric.incrementCounter(IEpicsMetricCounterType::ThrottlingIdleGauge, t_stat.idle_counter, {{"thread_id", thread_id}});
            metric.incrementCounter(IEpicsMetricCounterType::ThrottlingEventCounter, t_stat.total_events_processed, {{"thread_id", thread_id}});
            metric.incrementCounter(IEpicsMetricCounterType::ThrottlingDurationGauge, t_stat.total_idle_cycles, {{"thread_id", thread_id}});
            metric.incrementCounter(IEpicsMetricCounterType::ThrottleGauge, t_stat.throttle_us, {{"thread_id", thread_id}});

            throttling_info += STRING_FORMAT(
                "-Thr %1% - IdlCnt: %2%, ProcEvt: %3%, IdleCyc: %4%, TrtUsec: %5% us-",
                thread_id % t_stat.idle_counter % t_stat.total_events_processed % t_stat.total_idle_cycles % t_stat.throttle_us);
        }
    }
    // Log the statistics
    logger->logMessage(STRING_FORMAT("EPICS Monitor Statistics: Active PVs: %1%, Total PVs: %2%, Thread Throttling: %3%", active_pv % total_pv % throttling_info), LogLevel::TRACE);
}
