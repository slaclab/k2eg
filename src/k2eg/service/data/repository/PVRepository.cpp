#include <k2eg/service/data/DataStorage.h>
#include <k2eg/service/data/repository/PVRepository.h>

#include <iostream>

using namespace k2eg::service::data;
using namespace k2eg::service::data::repository;

using namespace sqlite_orm;

PVRepository::PVRepository(DataStorage& data_storage) : data_storage(data_storage) {}

void
PVRepository::insert(const PVMonitorType& pv_description) {
  auto locked_instance = data_storage.getLockedStorage();
  auto lstorage        = locked_instance.get();
  auto found           = lstorage->count<PVMonitorType>(
      where(c(&PVMonitorType::pv_name) == pv_description.pv_name and c(&PVMonitorType::pv_destination) == pv_description.pv_destination));
  if (!found) {
    // insert with the instance set
    PVMonitorType new_pv_description = pv_description;
    new_pv_description.requested_instance = 1;
    lstorage->insert(new_pv_description);
  } else {
    auto pv_monitors = lstorage->get_all<PVMonitorType>(
        where(c(&PVMonitorType::pv_name) == pv_description.pv_name and c(&PVMonitorType::pv_destination) == pv_description.pv_destination));
    // just in case w
    assert(pv_monitors.size() != 0);
    // we have to increase the instance number
    lstorage->update_all(set(c(&PVMonitorType::requested_instance) = pv_monitors.at(0).requested_instance + 1),
                         where(c(&PVMonitorType::pv_name) == pv_description.pv_name and c(&PVMonitorType::pv_destination) == pv_description.pv_destination));
  }
}

void
PVRepository::remove(const std::string& pv_name, const std::string& pv_destination) {
  auto locked_instance = data_storage.getLockedStorage();
  auto lstorage        = locked_instance.get();

  // get the pv monitor infor for check the instance counter
  auto pv_monitors = lstorage->get_all<PVMonitorType>(
      where(c(&PVMonitorType::pv_name) == pv_name and c(&PVMonitorType::pv_destination) == pv_destination));

  if (pv_monitors.size() == 0) { return; }
  // this should never happen
  assert(pv_monitors.at(0).requested_instance != 0);

  if (pv_monitors.at(0).requested_instance == 1) {
    // we have to delete
    lstorage->remove_all<PVMonitorType>(
        where(c(&PVMonitorType::pv_name) == pv_name and c(&PVMonitorType::pv_destination) == pv_destination));
  } else {
    // we have to decrement the instance count
    lstorage->update_all(set(c(&PVMonitorType::requested_instance) = pv_monitors.at(0).requested_instance - 1),
                         where(c(&PVMonitorType::pv_name) == pv_name and c(&PVMonitorType::pv_destination) == pv_destination));
  }
}

bool
PVRepository::isPresent(const std::string& pv_name, const std::string& pv_destination) const {
  auto result = data_storage.getLockedStorage().get()->count<PVMonitorType>(where(c(&PVMonitorType::pv_name) == pv_name and
                                                                                  c(&PVMonitorType::pv_destination) == pv_destination));
  return result != 0;
}

std::optional<PVMonitorTypeUPtr>
PVRepository::getPVMonitor(const std::string& pv_name, const std::string& pv_destination) const {
  auto result =
      data_storage.getLockedStorage().get()->get_all_pointer<PVMonitorType>(where(c(&PVMonitorType::pv_name) == pv_name
                                                                                  and c(&PVMonitorType::pv_destination) == pv_destination));
  return (result.size() == 0) ? std::optional<std::unique_ptr<PVMonitorType>>() : make_optional(std::make_unique<PVMonitorType>(*result[0]));
}

PVMonitorDistinctResultType
PVRepository::getDistinctByNameProtocol() const {
  auto locked_instance = data_storage.getLockedStorage();
  auto lstorage        = locked_instance.get();
  auto result          = lstorage->select(distinct(columns(&PVMonitorType::pv_name, &PVMonitorType::pv_protocol)));
  return result;
}

void
PVRepository::processAllPVMonitor(const std::string& pv_name, const std::string& pv_protocol, PVMonitorTypeProcessHandler handler) const {
  auto locked_instance = data_storage.getLockedStorage();
  auto lstorage        = locked_instance.get();
  for (uint32_t idx = 0; auto& pv_description : lstorage->iterate<PVMonitorType>(where(c(&PVMonitorType::pv_name) == pv_name))) {
    handler(idx++, pv_description);
  }
}

void
PVRepository::removeAll() {
  return data_storage.getLockedStorage().get()->remove_all<PVMonitorType>();
}