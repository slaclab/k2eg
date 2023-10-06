#ifndef k2eg_SERVICE_DATA_MODEL_CHANNEL_H_
#define k2eg_SERVICE_DATA_MODEL_CHANNEL_H_
#include <k2eg/controller/command/CMDCommand.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace k2eg::service::data {
class DataStorage;
namespace repository {
struct ChannelMonitorType {
    int id = -1;
    std::string pv_name;
    std::uint8_t event_serialization;
    std::string channel_protocol;
    std::string channel_destination;
    bool processed = false;
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
    ChannelMonitorDistinctResultType getDistinctByNameProtocol() const;
    void processAllChannelMonitor(const std::string& pv_name,
                                  ChannelMonitorTypeProcessHandler handler) const;
    // apply the handler to all non processed element
    void processUnprocessedChannelMonitor(const std::string& pv_name,
                                  const size_t number_of_element,
                                  ChannelMonitorTypeProcessHandler handler) const;
    void resetProcessStateChannel(const std::string& pv_name);
    void removeAll();
};
} // namespace repository
} // namespace k2eg::service::data

#endif // k2eg_SERVICE_DATA_MODEL_CHANNEL_H_
