#include <k2eg/service/data/DataStorage.h>
#include <k2eg/service/data/repository/ChannelRepository.h>

#include <chrono>
#include <iostream>

using namespace k2eg::service::data;
using namespace k2eg::service::data::repository;

using namespace sqlite_orm;

ChannelRepository::ChannelRepository(DataStorage& data_storage) : data_storage(data_storage) {}

bool
ChannelRepository::insert(const ChannelMonitorType& channel_description) {
  auto               locked_instance = data_storage.getLockedStorage();
  auto               lstorage        = locked_instance.get();
  ChannelMonitorType tmp_data        = channel_description;
  // set to default value the mandatory field for purging the monitor
  tmp_data.processed      = false;
  tmp_data.start_purge_ts = -1;

  auto channel_monitor = lstorage->get_all_pointer<ChannelMonitorType>(
      where(c(&ChannelMonitorType::pv_name) == channel_description.pv_name and
            c(&ChannelMonitorType::channel_destination) == channel_description.channel_destination));
  bool enabled = channel_monitor.size()==0;
  if (enabled) {
    lstorage->insert(tmp_data);
  } else {
     auto monitor_data    = lstorage->get<ChannelMonitorType>(channel_monitor[0]->id);
    // set purge ts to now
    monitor_data.counter++;
    //reset the purge timestamp because a new monitor has been request
    monitor_data.start_purge_ts = -1;
    // update monitor data
    lstorage->update(monitor_data);
  }
  return enabled;
}

void
ChannelRepository::remove(const ChannelMonitorType& channel_description) {
  auto locked_instance = data_storage.getLockedStorage();
  locked_instance.get()->remove_all<ChannelMonitorType>(where(c(&ChannelMonitorType::pv_name) == channel_description.pv_name and
                                                              c(&ChannelMonitorType::channel_destination) == channel_description.channel_destination));
}

bool
ChannelRepository::isPresent(const ChannelMonitorType& new_cannel) const {
  auto result = data_storage.getLockedStorage().get()->count<ChannelMonitorType>(
      where(c(&ChannelMonitorType::pv_name) == new_cannel.pv_name and c(&ChannelMonitorType::channel_destination) == new_cannel.channel_destination));
  return result != 0;
}

std::optional<ChannelMonitorTypeUPtr>
ChannelRepository::getChannelMonitor(const ChannelMonitorType& channel_descirption) const {
  auto result = data_storage.getLockedStorage().get()->get_all_pointer<ChannelMonitorType>(
      where(c(&ChannelMonitorType::pv_name) == channel_descirption.pv_name and
            c(&ChannelMonitorType::channel_destination) == channel_descirption.channel_destination));
  return (result.size() == 0) ? std::optional<std::unique_ptr<ChannelMonitorType>>() : make_optional(std::make_unique<ChannelMonitorType>(*result[0]));
}

std::optional<ChannelMonitorTypeUPtr>
ChannelRepository::getNextChannelMonitorToProcess(const std::string& pv_name) const {
  auto result = data_storage.getLockedStorage().get()->get_all_pointer<ChannelMonitorType>(
    where(
      c(&ChannelMonitorType::pv_name) == pv_name and
      c(&ChannelMonitorType::processed) == false
    ), limit(1));
  return (result.size() == 0) ? std::optional<std::unique_ptr<ChannelMonitorType>>() : make_optional(std::make_unique<ChannelMonitorType>(*result[0]));
}

ChannelMonitorDistinctResultType
ChannelRepository::getDistinctByNameProtocol() const {
  auto locked_instance = data_storage.getLockedStorage();
  auto lstorage        = locked_instance.get();
  auto result          = lstorage->select(distinct(columns(&ChannelMonitorType::pv_name)));
  return result;
}

void
ChannelRepository::processAllChannelMonitor(const std::string& pv_name, ChannelMonitorTypeProcessHandler handler) const {
  auto locked_instance = data_storage.getLockedStorage();
  auto lstorage        = locked_instance.get();
  for (uint32_t idx = 0; auto& channel_description : lstorage->iterate<ChannelMonitorType>(where(c(&ChannelMonitorType::pv_name) == pv_name))) {
    handler(idx++, channel_description);
  }
}

void
ChannelRepository::processUnprocessedChannelMonitor(const std::string&               pv_name,
                                                    const size_t                     number_of_element_to_process,
                                                    ChannelMonitorTypeProcessHandler handler) const {
  auto locked_instance = data_storage.getLockedStorage();
  auto lstorage        = locked_instance.get();
  for (uint32_t idx = 0; auto& channel_description : lstorage->iterate<ChannelMonitorType>(
                             where(c(&ChannelMonitorType::pv_name) == pv_name and c(&ChannelMonitorType::processed) == false))) {
    if (idx >= number_of_element_to_process) break;
    handler(idx++, channel_description);
    // update node as processed
    lstorage->update_all(set(c(&ChannelMonitorType::processed) = true), where(c(&ChannelMonitorType::id) == channel_description.id));
  }
}
void
ChannelRepository::resetProcessStateChannel(const std::string& pv_name) {
  auto locked_instance = data_storage.getLockedStorage();
  auto lstorage        = locked_instance.get();
  lstorage->update_all(set(c(&ChannelMonitorType::processed) = false), where(c(&ChannelMonitorType::pv_name) == pv_name));
}
void
ChannelRepository::setProcessStateChannel(const int64_t id, bool process_state) {
  auto locked_instance = data_storage.getLockedStorage();
  auto lstorage        = locked_instance.get();
  lstorage->update_all(set(c(&ChannelMonitorType::processed) = process_state), where(c(&ChannelMonitorType::id) == id));
}

void
ChannelRepository::setStartPurgeTimeStamp(int64_t monitor_id) {
  auto locked_instance = data_storage.getLockedStorage();
  auto lstorage        = locked_instance.get();
  auto monitor_data    = lstorage->get<ChannelMonitorType>(monitor_id);

  // set purge ts to now
  monitor_data.start_purge_ts = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  // update monitor data
  lstorage->update(monitor_data);
}

void
ChannelRepository::resetStartPurgeTimeStamp(int64_t monitor_id) {
  auto locked_instance = data_storage.getLockedStorage();
  auto lstorage        = locked_instance.get();
  auto monitor_data    = lstorage->get<ChannelMonitorType>(monitor_id);
  // set purge ts to now
  monitor_data.start_purge_ts = -1;
  // update monitor data
  lstorage->update(monitor_data);
}

void
ChannelRepository::removeAll() {
  return data_storage.getLockedStorage().get()->remove_all<ChannelMonitorType>();
}