#include <k2eg/controller/node/configuration/NodeConfiguration.h>
#include <k2eg/common/utility.h>
#include <cstddef>
#include <optional>
#include "k2eg/service/data/repository/ChannelRepository.h"

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

size_t NodeConfiguration::iterateAllChannelMonitor(size_t element_to_process, ChannelMonitorTypePurgeHandler purge_handler) {
    auto channel_repository = toShared(data_storage->getChannelRepository());
    auto distinct_name_prot = channel_repository->getDistinctByNameProtocol();
    size_t processed = 0;
    for(auto &ch: distinct_name_prot) {
        std::optional<ChannelMonitorTypeUPtr> found = channel_repository->getNextChannelMonitorToProcess(std::get<0>(ch));
        while(found.has_value()) {
            if(++processed > element_to_process) break;
            // 0-leave untouche, 1 set timestamp, -1 reset timestamp
            int purge_ts_set_flag = 0;
            purge_handler(*found->get(), purge_ts_set_flag);
            if(purge_ts_set_flag == -1) {
                // reset ts to purge
                channel_repository->resetStartPurgeTimeStamp(found->get()->id);
            } else if(purge_ts_set_flag == 1){
                // set the purge timestamp
                channel_repository->setStartPurgeTimeStamp(found->get()->id);
            }
            // tag as processed
            channel_repository->setProcessStateChannel(found->get()->id, true);
            // get next
            found = channel_repository->getNextChannelMonitorToProcess(std::get<0>(ch));
        }
    }
    return processed;
}

void NodeConfiguration::resetAllChannelMonitorCheck() {
    auto channel_repository = toShared(data_storage->getChannelRepository());
    auto distinct_name_prot = channel_repository->getDistinctByNameProtocol();
    for(auto &ch: distinct_name_prot) {
         channel_repository->resetProcessStateChannel(std::get<0>(ch));
    }
}