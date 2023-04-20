#include <chrono>
#include <k2eg/service/epics/EpicsServiceManager.h>
#include <ranges>
#include "k2eg/service/epics/EpicsPutOperation.h"

using namespace k2eg::common;
using namespace k2eg::service::epics_impl;

EpicsServiceManager::EpicsServiceManager() {
    run = true;
    pva_provider = std::make_unique<pvac::ClientProvider>("pva", epics::pvAccess::ConfigurationBuilder().push_env().build());
    ca_provider = std::make_unique<pvac::ClientProvider>("ca", 
    epics::pvAccess::ConfigurationBuilder()
    .add("PATH", "build/local/bin/linux-x86_64").push_map()
    .push_env().build());
    scheduler_thread = std::make_unique<std::thread>(&EpicsServiceManager::task, this);
}
EpicsServiceManager::~EpicsServiceManager() {
    run = false;
    scheduler_thread->join();
    pva_provider->reset();
    ca_provider->reset();
}

#define SELECT_PROVIDER(p) \
(protocol.find("pva") == 0?*pva_provider:*ca_provider)

void EpicsServiceManager::addChannel(const std::string& channel_name, const std::string& protocol) {
    std::unique_lock guard(channel_map_mutex);
    if (auto search = channel_map.find(channel_name); search != channel_map.end()) {
        return;
    }
    try {
        channel_map[channel_name] = std::make_shared<EpicsChannel>(SELECT_PROVIDER(protocol), channel_name);
        channel_map[channel_name]->startMonitor();
    } catch (std::exception& ex) {
        channel_map.erase(channel_name);
        throw ex;
    } catch (...) {
        channel_map.erase(channel_name);
        throw std::runtime_error("Unknown error during channel registration");
    }
}

StringVector EpicsServiceManager::getMonitoredChannels() {
    std::unique_lock guard(channel_map_mutex);
#if defined(__clang__)
    StringVector result;
    for (auto& p: channel_map) {
        result.push_back(p.first);
    }
    return result;
#elif  __GNUC_PREREQ(11,0)
    auto kv = std::views::keys(channel_map);
    return {kv.begin(), kv.end()};
#endif
}

void EpicsServiceManager::removeChannel(const std::string& channel_name) {
    std::lock_guard guard(channel_map_mutex);
    channel_map.erase(channel_name);
}

void EpicsServiceManager::monitorChannel(const std::string& channel_name, bool activate, const std::string& protocol) {
    if (activate) {
        addChannel(channel_name, protocol);
    } else {
        removeChannel(channel_name);
    }
}

ConstChannelDataUPtr EpicsServiceManager::getChannelData(const std::string& channel_name, const std::string& protocol) {
    // allocate channel and return data
    auto channel = std::make_unique<EpicsChannel>(SELECT_PROVIDER(protocol), channel_name);
    return channel->getChannelData();
}

ConstPutOperationUPtr EpicsServiceManager::putChannelData(const std::string& channel_name, const std::string& field, const std::string& value, const std::string& protocol) {
    // allocate channel and return data
    auto channel = std::make_unique<EpicsChannel>(SELECT_PROVIDER(protocol), channel_name);
    return channel->put(field, value);
}

size_t EpicsServiceManager::getChannelMonitoredSize() {
    std::lock_guard guard(channel_map_mutex);
    return channel_map.size();
}

k2eg::common::BroadcastToken EpicsServiceManager::addHandler(EpicsServiceManagerHandler new_handler) {
    std::lock_guard guard(channel_map_mutex);
    return handler_broadcaster.registerHandler(new_handler);
}

size_t EpicsServiceManager::getHandlerSize() {
    std::lock_guard guard(channel_map_mutex);
    handler_broadcaster.purge();
    return handler_broadcaster.targets.size();
}
void EpicsServiceManager::processIterator(const std::shared_ptr<EpicsChannel>& epics_channel) {
    MonitorEventVecShrdPtr received_event = epics_channel->monitor();
    if (!received_event->size() || !handler_broadcaster.targets.size()) return;
    handler_broadcaster.broadcast(received_event);
}

void EpicsServiceManager::task() {
    std::shared_ptr<EpicsChannel> current_channel;
    while (run) {
        // lock and scan opened channel
        {
            std::unique_lock guard(channel_map_mutex);
            for (auto& [key, value]: channel_map) {
                processIterator(value);
            }
        }
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}