#include <k2eg/common/utility.h>
#include <k2eg/service/epics/EpicsChannel.h>
#include <k2eg/service/epics/EpicsGetOperation.h>
#include <k2eg/service/epics/EpicsMonitorOperation.h>
#include <k2eg/service/epics/EpicsPutOperation.h>
#include <k2eg/service/epics/EpicsServiceManager.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <ranges>
#include <regex>

using namespace k2eg::common;
using namespace k2eg::service::epics_impl;

#define SELECT_PROVIDER(p) (p.find("pva") == 0 ? *pva_provider : *ca_provider)

EpicsServiceManager::EpicsServiceManager(ConstEpicsServiceManagerConfigUPtr config)
    : config(std::move(config)), end_processing(false), processing_pool(this->config->thread_count) {
  pva_provider = std::make_unique<pvac::ClientProvider>("pva", epics::pvAccess::ConfigurationBuilder().push_env().build());
  ca_provider  = std::make_unique<pvac::ClientProvider>("ca", epics::pvAccess::ConfigurationBuilder().push_env().build());
}
EpicsServiceManager::~EpicsServiceManager() {
  // remove all monitor handler
  end_processing = true;
  processing_pool.wait_for_tasks();
  // remove pending channel
  channel_map.clear();
  pva_provider->reset();
  ca_provider->reset();
}

void
EpicsServiceManager::addChannel(const std::string& pv_name_uri) {
  auto sanitized_pv = sanitizePVName(pv_name_uri);
  if (!sanitized_pv) return;

  try {
    {
      WriteLockCM write_lock(channel_map_mutex);
      if (auto search = channel_map.find(sanitized_pv->name); search != channel_map.end()) { return; }
      channel_map[sanitized_pv->name] = ChannelMapElement{
          .channel  = std::make_shared<EpicsChannel>(SELECT_PROVIDER(sanitized_pv->protocol), sanitized_pv->name),
          .to_force = false,
          .to_erase = false,
      };
    }
    {
      ReadLockCM write_lock(channel_map_mutex);
      ConstMonitorOperationShrdPtr monitor_operation = channel_map[sanitized_pv->name].channel->monitor();
      // lock and insert in queue
      processing_pool.push_task(&EpicsServiceManager::task, this, monitor_operation);
    }
  } catch (std::exception& ex) {
    channel_map.erase(sanitized_pv->name);
    throw ex;
  } catch (...) {
    channel_map.erase(sanitized_pv->name);
    throw std::runtime_error("Unknown error during channel registration");
  }
}

StringVector
EpicsServiceManager::getMonitoredChannels() {
  std::unique_lock guard(channel_map_mutex);
#if defined(__clang__)
  StringVector result;
  for (auto& p : channel_map) { result.push_back(p.first); }
  return result;
#elif __GNUC_PREREQ(11, 0)
  auto kv = std::views::keys(channel_map);
  return {kv.begin(), kv.end()};
#endif
}

void
EpicsServiceManager::removeChannel(const std::string& pv_name_uri) {
  auto sanitized_pv = sanitizePVName(pv_name_uri);
  if (!sanitized_pv) return;
  ReadLockCM read_lock(channel_map_mutex);
  if (auto search = channel_map.find(sanitized_pv->name); search != channel_map.end()) { search->second.to_erase = true; }
}

void
EpicsServiceManager::monitorChannel(const std::string& pv_identification, bool activate) {
  if (activate) {
    addChannel(pv_identification);
  } else {
    removeChannel(pv_identification);
  }
}

void
EpicsServiceManager::forceMonitorChannelUpdate(const std::string& pv_name_uri) {
  auto sanitized_pv = sanitizePVName(pv_name_uri);
  if (!sanitized_pv) return;
  ReadLockCM read_lock(channel_map_mutex);
  if (auto search = channel_map.find(sanitized_pv->name); search != channel_map.end()) { search->second.to_force = true; }
}

ConstGetOperationUPtr
EpicsServiceManager::getChannelData(const std::string& pv_name_uri) {
  ConstGetOperationUPtr result;
  WriteLockCM           write_lock(channel_map_mutex);
  auto                  sanitized_pv = sanitizePVName(pv_name_uri);
  // give a sanitization on pvname, the value will be not used cause k2eg return always all information
  if (!sanitized_pv) { return ConstGetOperationUPtr(); }
  if (auto search = channel_map.find(sanitized_pv->name); search == channel_map.end()) {
    channel_map[sanitized_pv->name] = ChannelMapElement{
          .channel  = std::make_shared<EpicsChannel>(SELECT_PROVIDER(sanitized_pv->protocol), sanitized_pv->name),
          .to_force = false,
          .to_erase = false,
    };
  }
    // allocate channel and return data
  result    = channel_map[sanitized_pv->name].channel->get();
  return result;
}

ConstPutOperationUPtr
EpicsServiceManager::putChannelData(const std::string& pv_name_uri, const std::string& value) {
  ConstPutOperationUPtr result;
  WriteLockCM           write_lock(channel_map_mutex);
  auto                  sanitized_pv = sanitizePVName(pv_name_uri);
  if (!sanitized_pv) { return ConstPutOperationUPtr(); }
  if (auto search = channel_map.find(sanitized_pv->name); search == channel_map.end()) {
    channel_map[sanitized_pv->name] = ChannelMapElement{
          .channel  = std::make_shared<EpicsChannel>(SELECT_PROVIDER(sanitized_pv->protocol), sanitized_pv->name),
          .to_force = false,
          .to_erase = false,
    };
  }
  // allocate channel and return data
  result = channel_map[sanitized_pv->name].channel->put(sanitized_pv->field, value);
  return result;
}

size_t
EpicsServiceManager::getChannelMonitoredSize() {
  std::lock_guard guard(channel_map_mutex);
  return channel_map.size();
}

k2eg::common::BroadcastToken
EpicsServiceManager::addHandler(EpicsServiceManagerHandler new_handler) {
  std::lock_guard guard(channel_map_mutex);
  return handler_broadcaster.registerHandler(new_handler);
}

size_t
EpicsServiceManager::getHandlerSize() {
  std::lock_guard guard(channel_map_mutex);
  handler_broadcaster.purge();
  return handler_broadcaster.targets.size();
}

// regex for IOC name
std::regex pv_name_regex("^(pva?|ca)://([a-zA-Z0-9-_]+(?::[a-zA-Z0-9-_]+)*)(\\.([a-zA-Z0-9-_]+(?:\\.[a-zA-Z0-9_]+)*))?$");
PVUPtr
EpicsServiceManager::sanitizePVName(const std::string& pv_name) {
  std::smatch match;
  std::string protocol;
  std::string base_pv_name;
  std::string field_name = "value";
  // Use std::regex_match to check if pvName matches regExp
  if (!std::regex_match(pv_name, match, pv_name_regex)) { return PVUPtr(); }

  size_t matches = match.size();
  // Access the groups: match[1] is the PV name without field, match[3] is the field including the '.'
  if (matches > 2) {
    protocol     = match[1].str();
    base_pv_name = match[2].str();
  }
  // try to decode the multy dot field part
  if (match.size() == 5) {
    std::string tmp = match[4].str();
    // Remove the '.' from the start of the field name
    if (!tmp.empty()) { field_name = tmp; }
  }
  return std::make_unique<PV>(PV{protocol, base_pv_name, field_name});
}

void
EpicsServiceManager::task(ConstMonitorOperationShrdPtr monitor_op) {
  if (end_processing) return;
  bool to_delete = false;
  {
    // lock the map for read and produce event
    ReadLockCM read_lock(channel_map_mutex);
    // try to fetch a specific number of 'data' event only

    if ((to_delete = channel_map[monitor_op->getPVName()].to_erase) == false) {
      // monitor can be processed becase is not goingto be deleted
      if (channel_map[monitor_op->getPVName()].to_force) {
        // i need to force the update
        monitor_op->forceUpdate();
        channel_map[monitor_op->getPVName()].to_force = false;
      }
      // execute pool on monitor
      monitor_op->poll();
      // if we have events fire them to listener
      if (monitor_op->hasEvents() && handler_broadcaster.targets.size()) { handler_broadcaster.broadcast(monitor_op->getEventData()); }
    }
  }

  if (to_delete) {
    WriteLockCM write_lock(channel_map_mutex);
    // remove the channel from map
    channel_map.erase(monitor_op->getPVName());
    // exit withous submiting again the task
    return;
  }
  // give some time of relaxing
  std::this_thread::sleep_for(std::chrono::microseconds(100));
  // re-enque
  processing_pool.push_task(&EpicsServiceManager::task, this, monitor_op);
}
