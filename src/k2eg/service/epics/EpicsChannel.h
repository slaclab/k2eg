#ifndef K2EG_SERVICE_EPICS_EPICSCHANNEL_H_
#define K2EG_SERVICE_EPICS_EPICSCHANNEL_H_

#include <cadef.h>
#include <k2eg/common/types.h>
#include <k2eg/service/epics/EpicsData.h>
#include <k2eg/service/epics/EpicsGetOperation.h>
#include <k2eg/service/epics/EpicsPutOperation.h>
#include <pv/configuration.h>
#include <pv/createRequest.h>
#include <pv/json.h>
#include <pva/client.h>

#include <memory>

namespace k2eg::service::epics_impl {

enum EventType { Fail, Cancel, Disconnec, Data };

typedef struct {
  EventType         type;
  const std::string message;
  ChannelData       channel_data;
} MonitorEvent;
DEFINE_PTR_TYPES(MonitorEvent)

typedef std::vector<MonitorEventShrdPtr> MonitorEventVec;
typedef std::shared_ptr<MonitorEventVec> MonitorEventVecShrdPtr;

struct EventReceived {
  MonitorEventVecShrdPtr event_data       = std::make_shared<MonitorEventVec>();
  MonitorEventVecShrdPtr event_fail       = std::make_shared<MonitorEventVec>();
  MonitorEventVecShrdPtr event_disconnect = std::make_shared<MonitorEventVec>();
  MonitorEventVecShrdPtr event_cancell    = std::make_shared<MonitorEventVec>();
};
DEFINE_PTR_TYPES(EventReceived)

class EpicsChannel {
  friend class EpicsPutOperation;
  const std::string                          channel_name;
  const std::string                          address;
  epics::pvData::PVStructure::shared_pointer pvReq = epics::pvData::createRequest("field()");
  // epics::pvAccess::Configuration::shared_pointer conf  = epics::pvAccess::ConfigurationBuilder().push_env().build();
  // std::unique_ptr<pvac::ClientProvider>          provider;
  std::shared_ptr<pvac::ClientChannel> channel;
  pvac::MonitorSync                    mon;

 public:
  explicit EpicsChannel(pvac::ClientProvider& provider, const std::string& channel_name, const std::string& address = std::string());
  ~EpicsChannel();
  static void                                      init();
  static void                                      deinit();
  ConstChannelDataUPtr                             getChannelData() const;
  epics::pvData::PVStructure::const_shared_pointer getData() const;
  ConstPutOperationUPtr                            put(const std::string& field, const std::string& value);
  ConstGetOperationUPtr                            get();
  void                                             startMonitor();
  EventReceivedShrdPtr                             monitor();
  void                                             stopMonitor();
};

DEFINE_PTR_TYPES(EpicsChannel)

}  // namespace k2eg::service::epics_impl
#endif
