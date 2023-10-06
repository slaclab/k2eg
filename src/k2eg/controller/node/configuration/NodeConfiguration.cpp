#include <k2eg/controller/node/configuration/NodeConfiguration.h>
#include <k2eg/common/utility.h>

using namespace k2eg::common;

using namespace k2eg::controller::node::configuration;

using namespace k2eg::service::data;
using namespace k2eg::service::data::repository;

NodeConfiguration::NodeConfiguration(DataStorageShrdPtr data_storage)
    : data_storage(data_storage) {}

std::vector<bool> NodeConfiguration::addChannelMonitor(const ChannelMonitorTypeConstVector& channel_descriptions) {
    auto channel_repository = toShared(data_storage->getChannelRepository());
    std::vector<bool> result(channel_descriptions.size());
    int idx = 0;
    for(auto const &desc: channel_descriptions) {
        result[idx++] = channel_repository->insert(desc);
    }
    return result;
}

void NodeConfiguration::removeChannelMonitor(const ChannelMonitorTypeConstVector& channel_descriptions) {
    auto channel_repository = toShared(data_storage->getChannelRepository());
    for(auto const &desc: channel_descriptions) {
        channel_repository->remove(desc);
    }
}

void NodeConfiguration::iterateAllChannelMonitor(ChannelMonitorTypeProcessHandler handler) {
    auto channel_repository = toShared(data_storage->getChannelRepository());
    auto distinct_name_prot = channel_repository->getDistinctByNameProtocol();
    for(auto &ch: distinct_name_prot) {
        channel_repository->processAllChannelMonitor(
            std::get<0>(ch),
            std::get<1>(ch),
            handler
        );
    }
}