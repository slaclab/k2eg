#include <k2eg/service/data/DataStorage.h>
#include <k2eg/service/data/repository/PVRepository.h>

#include <iostream>

using namespace k2eg::service::data;
using namespace k2eg::service::data::repository;

using namespace sqlite_orm;

PVRepository::PVRepository(DataStorage& data_storage)
    : data_storage(data_storage) {}

void PVRepository::insert(const PVMonitorType& pv_description) {
    auto locked_instance = data_storage.getLockedStorage();
    auto lstorage = locked_instance.get();
    auto found = lstorage->count<PVMonitorType>(
        where(c(&PVMonitorType::pv_name) == pv_description.pv_name
              and c(&PVMonitorType::pv_destination)
                      == pv_description.pv_destination));
    if (!found) lstorage->insert(pv_description);
}

void PVRepository::remove(const PVMonitorType& pv_description) {
    auto locked_instance = data_storage.getLockedStorage();
    locked_instance.get()->remove_all<PVMonitorType>(
        where(c(&PVMonitorType::pv_name) == pv_description.pv_name
              and c(&PVMonitorType::pv_destination)
                      == pv_description.pv_destination));
}

bool PVRepository::isPresent(const PVMonitorType& new_cannel) const {
    auto result = data_storage.getLockedStorage().get()->count<PVMonitorType>(
        where(c(&PVMonitorType::pv_name) == new_cannel.pv_name
              and c(&PVMonitorType::pv_protocol) == new_cannel.pv_protocol
              and c(&PVMonitorType::pv_destination)
                      == new_cannel.pv_destination));
    return result != 0;
}

std::optional<PVMonitorTypeUPtr> PVRepository::getPVMonitor(
    const PVMonitorType& pv_description) const {
    auto result =
        data_storage.getLockedStorage().get()->get_all_pointer<PVMonitorType>(
            where(c(&PVMonitorType::pv_name) == pv_description.pv_name
                //   and c(&PVMonitorType::pv_protocol)
                //           == pv_descirption.pv_protocol
                  and c(&PVMonitorType::pv_destination)
                          == pv_description.pv_destination));
    return (result.size() == 0)
               ? std::optional<std::unique_ptr<PVMonitorType>>()
               : make_optional(std::make_unique<PVMonitorType>(*result[0]));
}

PVMonitorDistinctResultType
PVRepository::getDistinctByNameProtocol() const {
    auto locked_instance = data_storage.getLockedStorage();
    auto lstorage = locked_instance.get();
    auto result =
        lstorage->select(distinct(columns(&PVMonitorType::pv_name,
                                          &PVMonitorType::pv_protocol)));
    return result;
}

void PVRepository::processAllPVMonitor(
    const std::string& pv_name,
    const std::string& pv_protocol,
    PVMonitorTypeProcessHandler handler) const {
    auto locked_instance = data_storage.getLockedStorage();
    auto lstorage = locked_instance.get();
    for (uint32_t idx = 0; auto& pv_description: lstorage->iterate<PVMonitorType>(
             where(c(&PVMonitorType::pv_name) == pv_name))) {
        handler(idx++, pv_description);
    }
}

void PVRepository::removeAll() {
    return data_storage.getLockedStorage().get()->remove_all<PVMonitorType>();
}