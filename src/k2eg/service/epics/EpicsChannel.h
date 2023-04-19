#ifndef K2EG_SERVICE_EPICS_EPICSCHANNEL_H_
#define K2EG_SERVICE_EPICS_EPICSCHANNEL_H_

#include <cadef.h>
#include <k2eg/common/types.h>
#include <k2eg/service/epics/EpicsData.h>
#include <k2eg/service/epics/EpicsPutOperation.h>
#include <pv/configuration.h>
#include <pv/createRequest.h>
#include <pv/json.h>
#include <pva/client.h>

#include <memory>

namespace k2eg::service::epics_impl {

enum MonitorType { Fail, Cancel, Disconnec, Data };

typedef struct {
  MonitorType       type;
  const std::string message;
  ChannelData       channel_data;
} MonitorEvent;
DEFINE_PTR_TYPES(MonitorEvent)

typedef std::vector<MonitorEventShrdPtr> MonitorEventVec;
typedef std::shared_ptr<MonitorEventVec> MonitorEventVecShrdPtr;

class EpicsChannel {
  friend class EpicsPutOperation;
  const std::string                              channel_name;
  const std::string                              address;
  epics::pvData::PVStructure::shared_pointer     pvReq = epics::pvData::createRequest("field()");
  epics::pvAccess::Configuration::shared_pointer conf  = epics::pvAccess::ConfigurationBuilder().push_env().build();
  std::unique_ptr<pvac::ClientProvider>          provider;
  std::shared_ptr<pvac::ClientChannel>           channel;
  pvac::MonitorSync                              mon;

 public:
  explicit EpicsChannel(const std::string& provider_name, const std::string& channel_name, const std::string& address = std::string());
  ~EpicsChannel();
  static void                                      init();
  static void                                      deinit();
  void                                             connect();
  ConstChannelDataUPtr                             getChannelData() const;
  epics::pvData::PVStructure::const_shared_pointer getData() const;
  template <typename T>
  void
  putData(const std::string& name, T new_value) const {
    channel->put().set(name, new_value).exec();
  }
  void                   putData(const std::string& name, const epics::pvData::AnyScalar& value) const;
  ConstPutOperationUPtr  putValue(const std::string& field, const std::string& value);
  void                   startMonitor();
  MonitorEventVecShrdPtr monitor();
  void                   stopMonitor();
};

DEFINE_PTR_TYPES(EpicsChannel)

}  // namespace k2eg::service::epics_impl
#endif
