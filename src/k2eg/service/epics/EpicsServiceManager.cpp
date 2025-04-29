#include <k2eg/common/utility.h>
#include <k2eg/common/BS_thread_pool.hpp>
#include <k2eg/service/epics/EpicsChannel.h>
#include <k2eg/service/epics/EpicsGetOperation.h>
#include <k2eg/service/epics/EpicsMonitorOperation.h>
#include <k2eg/service/epics/EpicsPutOperation.h>
#include <k2eg/service/epics/EpicsServiceManager.h>

#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <ranges>
#include <regex>

using namespace k2eg::common;
using namespace k2eg::service::epics_impl;

#define SELECT_PROVIDER(p) (p.find("pva") == 0 ? *pva_provider : *ca_provider)

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
{
    pva_provider = std::make_unique<pvac::ClientProvider>("pva", epics::pvAccess::ConfigurationBuilder().push_env().build());
    ca_provider = std::make_unique<pvac::ClientProvider>("ca", epics::pvAccess::ConfigurationBuilder().push_env().build());
}

EpicsServiceManager::~EpicsServiceManager()
{
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

    try
    {
        {
            WriteLockCM write_lock(channel_map_mutex);
            if (auto search = channel_map.find(sanitized_pv->name); search != channel_map.end())
            {
                return;
            }
            // Register a new channel for the sanitized PV name.
            channel_map[sanitized_pv->name] = ChannelMapElement{
                .channel = std::make_shared<EpicsChannel>(SELECT_PROVIDER(sanitized_pv->protocol), sanitized_pv->name),
                .to_force = false,
                .to_erase = false,
            };
        }
        {
            // Lock the channel map for reading and enqueue the monitor task.
            ReadLockCM                   read_lock(channel_map_mutex);
            ConstMonitorOperationShrdPtr monitor_operation = channel_map[sanitized_pv->name].channel->monitor();
            processing_pool->detach_task(
                [this, monitor_operation]
                {
                    this->task(monitor_operation);
                });
        }
    }
    catch (std::exception& ex)
    {
        // In case of an error, remove the faulty channel and rethrow.
        channel_map.erase(sanitized_pv->name);
        throw ex;
    }
    catch (...)
    {
        channel_map.erase(sanitized_pv->name);
        throw std::runtime_error("Unknown error during channel registration");
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
        search->second.to_erase = true;
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
    if (auto search = channel_map.find(sanitized_pv->name); search == channel_map.end())
    {
        channel_map[sanitized_pv->name] = ChannelMapElement{
            .channel = std::make_shared<EpicsChannel>(SELECT_PROVIDER(sanitized_pv->protocol), sanitized_pv->name),
            .to_force = false,
            .to_erase = false,
        };
    }
    result = channel_map[sanitized_pv->name].channel->monitor();
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
    if (auto search = channel_map.find(sanitized_pv->name); search == channel_map.end())
    {
        channel_map[sanitized_pv->name] = ChannelMapElement{
            .channel = std::make_shared<EpicsChannel>(SELECT_PROVIDER(sanitized_pv->protocol), sanitized_pv->name),
            .to_force = false,
            .to_erase = false,
        };
    }
    // allocate channel and return data
    result = channel_map[sanitized_pv->name].channel->get();
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
    if (auto search = channel_map.find(sanitized_pv->name); search == channel_map.end())
    {
        channel_map[sanitized_pv->name] = ChannelMapElement{
            .channel = std::make_shared<EpicsChannel>(SELECT_PROVIDER(sanitized_pv->protocol), sanitized_pv->name),
            .to_force = false,
            .to_erase = false,
        };
    }
    // allocate channel and return data
    result = channel_map[sanitized_pv->name].channel->put(sanitized_pv->field, value);
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

std::vector<ThreadThrottling> EpicsServiceManager::getThreadThrottlingInfo() const {
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
    auto&         throttling = thread_throttling_vector[thread_index.value()];
    {
        // Lock channel_map for reading to check if the current monitor should be deleted or updated.
        ReadLockCM read_lock(channel_map_mutex);
        auto       it = channel_map.find(pv_name);
        if (it == channel_map.end())
            return;

        auto& info = it->second;
        to_delete = info.to_erase;
        if (!to_delete)
        {
            if (info.to_force)
            {
                monitor_op->forceUpdate();
                info.to_force = false;
            }
            monitor_op->poll();
            if (monitor_op->hasEvents() && !handler_broadcaster.targets.empty())
            {
                handler_broadcaster.broadcast(monitor_op->getEventData());
                had_events = true;
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

    if (!had_events)
    {
        throttling.idle_counter++;
        throttling.total_idle_cycles++;
        if (throttling.idle_counter >= idle_threshold)
        {
            // Exponential backoff with max cap
            throttling.throttle_ms = std::min(throttling.throttle_ms * 2, max_throttle_ms);
            std::this_thread::sleep_for(std::chrono::milliseconds(throttling.throttle_ms));
            throttling.idle_counter = 0;
        }
    }
    else
    {
        // Work detected: reduce throttle or reset
        throttling.idle_counter = 0;
        throttling.total_events_processed++;
        throttling.throttle_ms = std::max(throttling.throttle_ms / 2, min_throttle_ms);
    }

    processing_pool->detach_task(
        [this, monitor_op]
        {
            this->task(monitor_op);
        });
}
