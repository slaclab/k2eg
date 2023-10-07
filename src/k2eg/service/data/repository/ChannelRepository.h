#ifndef k2eg_SERVICE_DATA_MODEL_CHANNEL_H_
#define k2eg_SERVICE_DATA_MODEL_CHANNEL_H_
#include <k2eg/controller/command/CMDCommand.h>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace k2eg::service::data {
class DataStorage;
namespace repository {
struct ChannelMonitorType {
    int64_t id = -1;
    std::string pv_name;
    std::uint8_t event_serialization;
    std::string channel_protocol;
    std::string channel_destination;
    bool processed = false;
    int64_t start_purge_ts = -1;
};

struct ChannelMonitorStat {
    int id = -1;
    int64_t mointor_id;
};

inline ChannelMonitorType toChannelMonitor(const k2eg::controller::command::cmd::MonitorCommand& acquire_command) {
    return ChannelMonitorType {
        .pv_name = acquire_command.pv_name, 
        .event_serialization = static_cast<std::uint8_t>(acquire_command.serialization),
        .channel_protocol = acquire_command.protocol,
        .channel_destination = acquire_command.monitor_destination_topic
    };
}

inline  k2eg::controller::command::cmd::ConstCommandShrdPtr  fromChannelMonitor(const ChannelMonitorType& command) {
    return std::make_shared<k2eg::controller::command::cmd::MonitorCommand>(k2eg::controller::command::cmd::MonitorCommand {
        k2eg::controller::command::cmd::CommandType::monitor,
         static_cast<k2eg::common::SerializationType>(command.event_serialization),
        command.channel_protocol,
        command.pv_name, 
        true,
        "",
        "",
        command.channel_destination
    });
}

typedef std::unique_ptr<ChannelMonitorType> ChannelMonitorTypeUPtr;
typedef std::function<void(uint32_t index, const ChannelMonitorType&)> ChannelMonitorTypeProcessHandler;
typedef std::vector<std::tuple<std::string, std::string>> ChannelMonitorDistinctResultType;
class ChannelRepository {
    friend class k2eg::service::data::DataStorage;
    DataStorage& data_storage;
    ChannelRepository(DataStorage& data_storage);

public:
    ~ChannelRepository() = default;
    bool insert(const ChannelMonitorType& channel_description);
    void remove(const ChannelMonitorType& channel_description);
    bool isPresent(const ChannelMonitorType& new_cannel) const;
    std::optional<ChannelMonitorTypeUPtr> getChannelMonitor(const ChannelMonitorType& channel_descirption) const;
    // return the next monitor data to process
    std::optional<ChannelMonitorTypeUPtr> getNextChannelMonitorToProcess(const std::string& pv_name) const;
    ChannelMonitorDistinctResultType getDistinctByNameProtocol() const;
    void processAllChannelMonitor(const std::string& pv_name,
                                  ChannelMonitorTypeProcessHandler handler) const;
    // apply the handler to all non processed element
    void processUnprocessedChannelMonitor(const std::string& pv_name,
                                  const size_t number_of_element,
                                  ChannelMonitorTypeProcessHandler handler) const;
    void resetProcessStateChannel(const std::string& pv_name);
    // set the 'start_purge_ts' filed to tag the beginning of purge check
    void setStartPurgeTimeStamp(int64_t monitor_id);
    // reset the purge check
    void resetStartPurgeTimeStamp(int64_t monitor_id);
    // remove all stored monitor
    void removeAll();
};
} // namespace repository
} // namespace k2eg::service::data

#endif // k2eg_SERVICE_DATA_MODEL_CHANNEL_H_
