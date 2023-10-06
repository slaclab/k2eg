#include <k2eg/service/data/DataStorage.h>
#include <k2eg/service/data/repository/ChannelRepository.h>

#include <iostream>

using namespace k2eg::service::data;
using namespace k2eg::service::data::repository;

using namespace sqlite_orm;

ChannelRepository::ChannelRepository(DataStorage& data_storage) : data_storage(data_storage) {}

bool
ChannelRepository::insert(const ChannelMonitorType& channel_description) {
  auto locked_instance = data_storage.getLockedStorage();
  auto lstorage        = locked_instance.get();
  auto found           = lstorage->count<ChannelMonitorType>(where(c(&ChannelMonitorType::pv_name) == channel_description.pv_name and
                                                         c(&ChannelMonitorType::channel_destination) == channel_description.channel_destination));

  if (!found) lstorage->insert(channel_description);
  return !found;
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
      where(c(&ChannelMonitorType::pv_name) == new_cannel.pv_name and c(&ChannelMonitorType::channel_protocol) == new_cannel.channel_protocol and
            c(&ChannelMonitorType::channel_destination) == new_cannel.channel_destination));
  return result != 0;
}

std::optional<ChannelMonitorTypeUPtr>
ChannelRepository::getChannelMonitor(const ChannelMonitorType& channel_descirption) const {
  auto result = data_storage.getLockedStorage().get()->get_all_pointer<ChannelMonitorType>(
      where(c(&ChannelMonitorType::pv_name) == channel_descirption.pv_name
            //   and c(&ChannelMonitorType::channel_protocol)
            //           == channel_descirption.channel_protocol
            and c(&ChannelMonitorType::channel_destination) == channel_descirption.channel_destination));
  return (result.size() == 0) ? std::optional<std::unique_ptr<ChannelMonitorType>>() : make_optional(std::make_unique<ChannelMonitorType>(*result[0]));
}

ChannelMonitorDistinctResultType
ChannelRepository::getDistinctByNameProtocol() const {
  auto locked_instance = data_storage.getLockedStorage();
  auto lstorage        = locked_instance.get();
  auto result          = lstorage->select(distinct(columns(&ChannelMonitorType::pv_name, &ChannelMonitorType::channel_protocol)));
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
  lstorage->update_all(set(c(&ChannelMonitorType::processed) = true), where(c(&ChannelMonitorType::pv_name) == pv_name));
}

void
ChannelRepository::removeAll() {
  return data_storage.getLockedStorage().get()->remove_all<ChannelMonitorType>();
}