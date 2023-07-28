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

EpicsServiceManager::EpicsServiceManager() : end_processing(false), processing_pool(2) {
  pva_provider = std::make_unique<pvac::ClientProvider>("pva", epics::pvAccess::ConfigurationBuilder().push_env().build());
  ca_provider  = std::make_unique<pvac::ClientProvider>("ca", epics::pvAccess::ConfigurationBuilder().push_env().build());
}
EpicsServiceManager::~EpicsServiceManager() {
  // remove all monitor handler
  end_processing = true;
  processing_pool.wait_for_tasks();
  pva_provider->reset();
  ca_provider->reset();
}

#define SELECT_PROVIDER(p) (protocol.find("pva") == 0 ? *pva_provider : *ca_provider)

void
EpicsServiceManager::addChannel(const std::string& pv_name, const std::string& protocol) {
  std::unique_lock guard(channel_map_mutex);
  if (auto search = channel_map.find(pv_name); search != channel_map.end()) { return; }
  try {
    channel_map[pv_name]                           = std::make_shared<EpicsChannel>(SELECT_PROVIDER(protocol), pv_name);
    ConstMonitorOperationShrdPtr monitor_operation = channel_map[pv_name]->monitor();
    // lock and insert in queue
    processing_pool.push_task(&EpicsServiceManager::task, this, monitor_operation);
  } catch (std::exception& ex) {
    channel_map.erase(pv_name);
    throw ex;
  } catch (...) {
    channel_map.erase(pv_name);
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
EpicsServiceManager::removeChannel(const std::string& pv_name) {
  std::lock_guard guard(channel_map_mutex);
  channel_map.erase(pv_name);

  std::lock_guard<std::mutex> lock(monitor_op_queue_mutx);
  pv_to_remove.insert(pv_name);
}

void
EpicsServiceManager::monitorChannel(const std::string& pv_name, bool activate, const std::string& protocol) {
  if (activate) {
    addChannel(pv_name, protocol);
  } else {
    removeChannel(pv_name);
  }
}

ConstGetOperationUPtr
EpicsServiceManager::getChannelData(const std::string& pv_name, const std::string& protocol) {
  ConstGetOperationUPtr result;
  std::unique_lock      guard(channel_map_mutex);
  // give a sanitization on pvname, the value will be not used cause k2eg return always all information
  auto pv = sanitizePVName(pv_name);
  if (auto search = channel_map.find(pv->name); search != channel_map.end()) {
    // the same channel is in monitor so we can use it
    result = search->second->get();
  } else {
    // allocate channel and return data
    auto channel = std::make_unique<EpicsChannel>(SELECT_PROVIDER(protocol), pv->name);
    result       = channel->get();
  }
  return result;
}

ConstPutOperationUPtr
EpicsServiceManager::putChannelData(const std::string& pv_name, const std::string& value, const std::string& protocol) {
  ConstPutOperationUPtr result;
  std::unique_lock      guard(channel_map_mutex);

  auto pv = sanitizePVName(pv_name);
  if (auto search = channel_map.find(pv->name); search != channel_map.end()) {
    // the same channel is in monitor so we can use it
    result = search->second->put(pv->field, value);
  } else {
    // allocate channel and return data
    auto channel = std::make_unique<EpicsChannel>(SELECT_PROVIDER(protocol), pv->name);
    result       = channel->put(pv->field, value);
  }
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

void
EpicsServiceManager::task(ConstMonitorOperationShrdPtr monitor_op) {
  if (end_processing) return;
  // fetch op to manage
  // std::this_thread::sleep_for(std::chrono::microseconds(100));
  {
    // check if we need to remove th emonitor for the specific pv owned by cur_monitor_op
    std::lock_guard<std::mutex> lock(monitor_op_queue_mutx);
    auto                        found =
        std::find_if(std::begin(pv_to_remove), std::end(pv_to_remove), [&monitor_op](auto& pv_name) { return monitor_op->getPVName().compare(pv_name) == 0; });
    if (found != std::end(pv_to_remove)) {
      // i need to remove this monitor
      pv_to_remove.erase(found);
      return;
    }
  }

  // manage monitor op
  std::lock_guard guard(channel_map_mutex);
  // try to fetch a specific number of 'data' event only
  monitor_op->poll();
  if (monitor_op->hasEvents() && handler_broadcaster.targets.size()) { handler_broadcaster.broadcast(monitor_op->getEventData()); }

  // re-enque
  processing_pool.push_task(&EpicsServiceManager::task, this, monitor_op);
}

// regex for IOC name
std::regex pv_name_regex("^([a-zA-Z0-9_]+(?::[a-zA-Z0-9_]+)*)(\\.([a-zA-Z0-9_]+(?:\\.[a-zA-Z0-9_]+)*))?$");

PVUPtr
EpicsServiceManager::sanitizePVName(const std::string& pv_name) {
  std::smatch match;
  std::string base_pv_name;
  std::string field_name = "value";
  // Use std::regex_match to check if pvName matches regExp
  if (std::regex_match(pv_name, match, pv_name_regex)) {
    size_t matches = match.size();
    // Access the groups: match[1] is the PV name without field, match[3] is the field including the '.'
    if (matches > 1) { base_pv_name = match[1].str(); }
    // try to decode the multy dot field part
    if (match.size() == 4) {
      std::string tmp = match[3].str();
      // Remove the '.' from the start of the field name
      if (!tmp.empty()) { field_name = tmp; }
    }
  }
  return std::make_unique<PV>(PV{base_pv_name, field_name});
}