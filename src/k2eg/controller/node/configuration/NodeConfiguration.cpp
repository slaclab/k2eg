#include <k2eg/common/utility.h>
#include <k2eg/controller/node/configuration/NodeConfiguration.h>
#include <k2eg/service/ServiceResolver.h>
#include <k2eg/service/configuration/INodeConfiguration.h>
#include <k2eg/service/configuration/configuration.h>
#include <k2eg/service/data/repository/ChannelRepository.h>
#include <k2eg/service/log/ILogger.h>

#include <cstddef>
#include <optional>

using namespace k2eg::common;

using namespace k2eg::controller::node::configuration;

using namespace k2eg::service;
using namespace k2eg::service::log;
using namespace k2eg::service::data;
using namespace k2eg::service::data::repository;

namespace sc = k2eg::service::configuration;

NodeConfiguration::NodeConfiguration(DataStorageShrdPtr data_storage)
    : data_storage(data_storage), node_configuration_service(ServiceResolver<sc::INodeConfiguration>::resolve()) {}

void
NodeConfiguration::loadNodeConfiguration() {
  auto logger = ServiceResolver<ILogger>::resolve();
  logger->logMessage("Fetch the node configuration", LogLevel::INFO);
  // load the fulll configuration
  node_configuration = node_configuration_service->getNodeConfiguration();

  if (node_configuration == nullptr) {
    logger->logMessage("Node configuration not found", LogLevel::ERROR);
    return;
  }
  // load the channel monitor
  ChannelMonitorTypeConstVector channel_monitor_vector;
  auto                          channel_repository = toShared(data_storage->getChannelRepository());
  auto                          channel_monitor    = node_configuration->pv_monitor_info_map;
  for (auto const &desc : channel_monitor) {
    auto data_insert_res = channel_repository->insert(ChannelMonitorType{.pv_name             = desc.first,
                                                                         .event_serialization = static_cast<std::uint8_t>(desc.second->event_serialization),
                                                                         .channel_destination = desc.second->pv_destination_topic});
    logger->logMessage(STRING_FORMAT("Loading pv %1% - ser: %2% - des: %3% stored: %4%",
                                     desc.first % desc.second->event_serialization % desc.second->pv_destination_topic % data_insert_res),
                       LogLevel::DEBUG);
  }
}

std::vector<bool>
NodeConfiguration::addChannelMonitor(const ChannelMonitorTypeConstVector &channel_descriptions) {
  auto              channel_repository = toShared(data_storage->getChannelRepository());
  std::vector<bool> result(channel_descriptions.size());
  int               idx = 0;

  for (auto const &desc : channel_descriptions) {
    // check if the channel is already present on the configuration
    auto new_conf_for_pv = sc::MakePVMonitorInfoShrdPtr(sc::PVMonitorInfo{
        .pv_destination_topic = desc.channel_destination, 
        .event_serialization = static_cast<std::uint8_t>(desc.event_serialization)
    });
    if(!node_configuration->isPresent(desc.pv_name, *new_conf_for_pv)) {
        // add new configuration to global
        node_configuration->pv_monitor_info_map.insert(sc::PVMonitorInfoMapPair(desc.pv_name, new_conf_for_pv));
        // register the new channel monitor into local database
        result[idx++] = channel_repository->insert(desc);
    } else {
        result[idx++] = false;
    }
  }

  // store the configuration on the server
  node_configuration_service->setNodeConfiguration(node_configuration);
  // return the result
  return result;
}

void
NodeConfiguration::removeChannelMonitor(const ChannelMonitorTypeConstVector &channel_descriptions) {
  auto channel_repository = toShared(data_storage->getChannelRepository());
  for (auto const &desc : channel_descriptions) { channel_repository->remove(desc); }
}

size_t
NodeConfiguration::iterateAllChannelMonitor(size_t element_to_process, ChannelMonitorTypeHandler handle) {
  auto   channel_repository = toShared(data_storage->getChannelRepository());
  auto   distinct_name_prot = channel_repository->getDistinctByNameProtocol();
  size_t processed          = 0;
  for (auto &ch : distinct_name_prot) {
    std::optional<ChannelMonitorTypeUPtr> found = channel_repository->getNextChannelMonitorToProcess(std::get<0>(ch));
    while (found.has_value()) {
      if (++processed > element_to_process) break;
      int dummy_flag = 0;
      handle(*found->get(), dummy_flag);
      // tag as processed
      channel_repository->setProcessStateChannel(found->get()->id, true);
      // get next
      found = channel_repository->getNextChannelMonitorToProcess(std::get<0>(ch));
    }
  }
  return processed;
}

size_t
NodeConfiguration::iterateAllChannelMonitorForAction(size_t element_to_process, ChannelMonitorTypeHandler purge_handler) {
  auto   channel_repository = toShared(data_storage->getChannelRepository());
  auto   distinct_name_prot = channel_repository->getDistinctByNameProtocol();
  size_t processed          = 0;
  for (auto &ch : distinct_name_prot) {
    std::optional<ChannelMonitorTypeUPtr> found = channel_repository->getNextChannelMonitorToProcess(std::get<0>(ch));
    while (found.has_value()) {
      if (++processed > element_to_process) break;
      // 0-leave untouche, 1 set timestamp, -1 reset timestamp
      int purge_ts_set_flag = 0;
      purge_handler(*found->get(), purge_ts_set_flag);
      if (purge_ts_set_flag == -1) {
        // reset ts to purge
        channel_repository->resetStartPurgeTimeStamp(found->get()->id);
      } else if (purge_ts_set_flag == 1) {
        // set the purge timestamp
        channel_repository->setStartPurgeTimeStamp(found->get()->id);
      }
      // tag as processed
      channel_repository->setProcessStateChannel(found->get()->id, true);
      // get next
      found = channel_repository->getNextChannelMonitorToProcess(std::get<0>(ch));
    }
  }
  // cleer the distinct name
  distinct_name_prot.clear();
  return processed;
}

void
NodeConfiguration::resetAllChannelMonitorCheck() {
  auto channel_repository = toShared(data_storage->getChannelRepository());
  auto distinct_name_prot = channel_repository->getDistinctByNameProtocol();
  for (auto &ch : distinct_name_prot) { channel_repository->resetProcessStateChannel(std::get<0>(ch)); }
}