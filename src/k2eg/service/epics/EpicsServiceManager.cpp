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
#include <mutex>
#include <ranges>
#include <regex>

using namespace k2eg::common;

using namespace k2eg::service::scheduler;
using namespace k2eg::service::epics_impl;
using namespace k2eg::service::metric;

#define SELECT_PROVIDER(p) (p.find("pva") == 0 ? *pva_provider : *ca_provider)
#define EPICS_MANAGER_STAT_TASK_NAME "epics-manager-stat-task"
#define EPICS_MANAGER_STAT_TASK_CRON "* * * * * *"

void set_thread_name(const std::size_t idx)
{
    const std::string name = "EPICS Monitor " + std::to_string(idx);
    const bool        result = BS::this_thread::set_os_thread_name(name);
}

EpicsServiceManager::EpicsServiceManager(ConstEpicsServiceManagerConfigUPtr config)
    : config(std::move(config))
    , end_processing(false)
    , thread_throttling_vector(this->config->thread_count)
    , processing_pool(std::make_shared<BS::light_thread_pool>(this->config->thread_count, set_thread_name))
    , metric(ServiceResolver<IMetricService>::resolve()->getEpicsMetric())
{
    pva_provider = std::make_unique<pvac::ClientProvider>("pva", epics::pvAccess::ConfigurationBuilder().push_env().build());
    ca_provider = std::make_unique<pvac::ClientProvider>("ca", epics::pvAccess::ConfigurationBuilder().push_env().build());
    auto statistic_task = MakeTaskShrdPtr(EPICS_MANAGER_STAT_TASK_NAME, // name of the task
                                          EPICS_MANAGER_STAT_TASK_CRON, // cron expression
                                          std::bind(&EpicsServiceManager::handleStatistic, this, std::placeholders::_1), // task handler
                                          -1 // start at application boot time
    );
    ServiceResolver<Scheduler>::resolve()->addTask(statistic_task);
}

EpicsServiceManager::~EpicsServiceManager()
{
    // stop the statistic
    bool erased = ServiceResolver<Scheduler>::resolve()->removeTaskByName(EPICS_MANAGER_STAT_TASK_NAME);
    // Stop processing monitor tasks and wait for scheduled tasks to finish.
    end_processing = true;
    processing_pool->wait();
    // Clear all registered channels and reset providers.
    channel_map.clear();
    pva_provider->reset();
    ca_provider->reset();
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
    std::unique_lock guard(channel_map_mutex);
    auto             kv = channel_map | std::views::transform(
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

void EpicsServiceManager::forceMonitorChannelUpdate(const std::string& pv_name_uri)
{
    auto sanitized_pv = sanitizePVName(pv_name_uri);
    if (!sanitized_pv)
        return;
    ReadLockCM read_lock(channel_map_mutex);
    if (auto search = channel_map.find(sanitized_pv->name); search != channel_map.end())
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
        };
        result = channel.channel->get();
    }
    return result;
}

ConstPutOperationUPtr EpicsServiceManager::putChannelData(const std::string& pv_name_uri, const std::string& value)
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
        result = channel_map[sanitized_pv->name].channel->put(sanitized_pv->field, value);
    }
    else
    {
        auto channel = ChannelMapElement{
            .channel = std::make_shared<EpicsChannel>(SELECT_PROVIDER(sanitized_pv->protocol), sanitized_pv->name),
            .to_force = false,
            .keep_alive = 0,
        };
        result = channel.channel->put(sanitized_pv->field, value);
    }
    return result;
}

size_t EpicsServiceManager::getChannelMonitoredSize()
{
    std::lock_guard guard(channel_map_mutex);
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
    bool        to_delete = false;
    const auto& pv_name = monitor_op->getPVName();
    auto&       throttling = thread_throttling_vector[thread_index.value()];
    {
        // Lock channel_map for reading to check if the current monitor should be deleted or updated.
        ChannelMapIterator it;
        {
            ReadLockCM read_lock(channel_map_mutex);
            it = channel_map.find(pv_name);
            if (it == channel_map.end())
            {
                return;
            }
        }

        // Copy info pointer to avoid holding reference after lock
        auto* info = &(it->second);

        to_delete = info->keep_alive == 0;
        if (!to_delete)
        {
            if (info->to_force)
            {
                monitor_op->forceUpdate();
                info->to_force = false;
            }
            monitor_op->poll();
            if (monitor_op->hasEvents() && !handler_broadcaster.targets.empty())
            {
                auto received_event = monitor_op->getEventData();
                had_events = true;
                // check if the channel is active or not
                info->active = received_event->event_data->empty() &&
                               (!received_event->event_cancel->empty() || !received_event->event_disconnect->empty() ||
                                !received_event->event_fail->empty());
                metric.incrementCounter(IEpicsMetricCounterType::MonitorData, received_event->event_data->size());
                metric.incrementCounter(IEpicsMetricCounterType::MonitorCancel, received_event->event_cancel->size());
                metric.incrementCounter(IEpicsMetricCounterType::MonitorDisconnect, received_event->event_disconnect->size());
                metric.incrementCounter(IEpicsMetricCounterType::MonitorFail, received_event->event_fail->size());
                // broadcast to handlers
                handler_broadcaster.broadcast(received_event);
            }
        }
    }

    if (to_delete)
    {
        WriteLockCM write_lock(channel_map_mutex);
        // Remove the channel from the map if marked for deletion.
        channel_map.erase(monitor_op->getPVName());
        return; // Exit without resubmitting task.
    }

    // manage throtling on situation where to much pv returtn nothing to reduce pressure
    constexpr int min_throttle_ms = 1;
    constexpr int max_throttle_ms = 100;
    constexpr int idle_threshold = 10; // how many idle cycles before backoff increases

    // manqage throtling
    throttling.update(had_events);

    // resubmit the task to the thread pool
    processing_pool->detach_task(
        [this, monitor_op]
        {
            this->task(monitor_op);
        });
}

void EpicsServiceManager::handleStatistic(TaskProperties& task_properties)
{
    ReadLockCM read_lock(channel_map_mutex);
    int        active_pv = std::count_if(std::execution::par, channel_map.begin(), channel_map.end(),
                                         [](const auto& pair)
                                         {
                                      return pair.second.active == true;
                                  });

    metric.incrementCounter(IEpicsMetricCounterType::ActiveMonitor, active_pv);
    metric.incrementCounter(IEpicsMetricCounterType::TotalMonitor, channel_map.size());

    auto& thread_throttling_vector = getThreadThrottlingInfo();
    for (std::size_t i = 0; i < thread_throttling_vector.size(); ++i)
    {
        const auto& throttling = thread_throttling_vector[i];
        std::string thread_id = std::to_string(i);
        auto        t_stat = throttling.getStats();
        metric.incrementCounter(IEpicsMetricCounterType::ThrottlingIdleCounter, t_stat.idle_counter, {{"thread_id", thread_id}});
        metric.incrementCounter(IEpicsMetricCounterType::ThrottlingEventCounter, t_stat.total_events_processed, {{"thread_id", thread_id}});
        metric.incrementCounter(IEpicsMetricCounterType::ThrottlingDurationCounter, t_stat.total_idle_cycles, {{"thread_id", thread_id}});
        metric.incrementCounter(IEpicsMetricCounterType::ThrottleGauge, t_stat.throttle_ms, {{"thread_id", thread_id}});
    }
}