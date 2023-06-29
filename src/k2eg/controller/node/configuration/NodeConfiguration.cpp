#include <k2eg/controller/node/configuration/NodeConfiguration.h>
#include <k2eg/common/utility.h>

using namespace k2eg::common;

using namespace k2eg::controller::node::configuration;

using namespace k2eg::service::data;
using namespace k2eg::service::data::repository;

NodeConfiguration::NodeConfiguration(DataStorageUPtr data_storage)
    : data_storage(std::move(data_storage)) {}

void NodeConfiguration::addPVMonitor(const PVMonitorTypeConstVector& pv_descriptions) {
    auto pv_repository = toShared(data_storage->getPVRepository());
    
    for(auto const &desc: pv_descriptions) {
        pv_repository->insert(desc);
    }
}

void NodeConfiguration::removePVMonitor(const PVMonitorTypeConstVector& pv_descriptions) {
    auto pv_repository = toShared(data_storage->getPVRepository());
    for(auto const &desc: pv_descriptions) {
        pv_repository->remove(desc.pv_name, desc.pv_destination);
    }
}

void NodeConfiguration::iterateAllPVMonitor(PVMonitorTypeProcessHandler handler) {
    auto pv_repository = toShared(data_storage->getPVRepository());
    auto distinct_name_prot = pv_repository->getDistinctByNameProtocol();
    for(auto &ch: distinct_name_prot) {
        pv_repository->processAllPVMonitor(
            std::get<0>(ch),
            std::get<1>(ch),
            handler
        );
    }
}