#ifndef k2eg_SERVICE_DATA_MODEL_PV_H_
#define k2eg_SERVICE_DATA_MODEL_PV_H_
#include <k2eg/controller/command/CMDCommand.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace k2eg::service::data {
class DataStorage;
namespace repository {
struct PVMonitorType {
  int           id = -1;
  std::string   pv_name;
  std::uint8_t  event_serialization;
  std::string   pv_protocol;
  std::string   pv_destination;
  std::uint16_t requested_instance;
};

inline PVMonitorType
toPVMonitor(const k2eg::controller::command::cmd::MonitorCommand& acquire_command) {
  return PVMonitorType{.pv_name             = acquire_command.pv_name,
                       .event_serialization = static_cast<std::uint8_t>(acquire_command.serialization),
                       .pv_protocol         = acquire_command.protocol,
                       .pv_destination      = acquire_command.destination_topic};
}

inline k2eg::controller::command::cmd::ConstCommandShrdPtr
fromPVMonitor(const PVMonitorType& command) {
  return std::make_shared<k2eg::controller::command::cmd::MonitorCommand>(
      k2eg::controller::command::cmd::MonitorCommand{k2eg::controller::command::cmd::CommandType::monitor,
                                                     static_cast<k2eg::common::SerializationType>(command.event_serialization),
                                                     command.pv_protocol,
                                                     command.pv_name,
                                                     true,
                                                     command.pv_destination});
}

typedef std::unique_ptr<PVMonitorType>                            PVMonitorTypeUPtr;
typedef std::function<void(uint32_t index, const PVMonitorType&)> PVMonitorTypeProcessHandler;
typedef std::vector<std::tuple<std::string, std::string>>         PVMonitorDistinctResultType;
class PVRepository {
  friend class k2eg::service::data::DataStorage;
  DataStorage& data_storage;
  PVRepository(DataStorage& data_storage);

 public:
  ~PVRepository() = default;
  void                             insert(const PVMonitorType& pv_description);
  void                             remove(const std::string& pv_name, const std::string& pv_destination);
  bool                             isPresent(const std::string& pv_name, const std::string& pv_destination) const;
  std::optional<PVMonitorTypeUPtr> getPVMonitor(const std::string& pv_name, const std::string& pv_destination) const;
  PVMonitorDistinctResultType      getDistinctByNameProtocol() const;
  void                             processAllPVMonitor(const std::string& pv_name, const std::string& pv_protocol, PVMonitorTypeProcessHandler handler) const;
  void                             removeAll();
};
}  // namespace repository
}  // namespace k2eg::service::data

#endif  // k2eg_SERVICE_DATA_MODEL_PV_H_
