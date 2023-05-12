#ifndef K2EG_SERVICE_EPICS_EPICSMONITOROPERATION_H_
#define K2EG_SERVICE_EPICS_EPICSMONITOROPERATION_H_

#include <k2eg/common/types.h>
#include <k2eg/service/epics/EpicsData.h>
#include <pva/client.h>

#include <mutex>

namespace k2eg::service::epics_impl {
// async monitor operation
class MonitorOperation : public pvac::ClientChannel::MonitorCallback {
  const std::string                    field;
  const std::string                    pv_name;
  pvac::Monitor                        mon;
  std::shared_ptr<pvac::ClientChannel> channel;
  EventReceivedShrdPtr                 consumed_event;
  mutable std::mutex                   ce_mtx;

 public:
  MonitorOperation(std::shared_ptr<pvac::ClientChannel> channel, const std::string& pv_name, const std::string& field = "field()");
  virtual ~MonitorOperation();

  virtual void         monitorEvent(const pvac::MonitorEvent& evt) OVERRIDE FINAL;
  EventReceivedShrdPtr getEventData() const;
  bool                 hasData() const;
};
}  // namespace k2eg::service::epics_impl

#endif  // K2EG_SERVICE_EPICS_EPICSMONITOROPERATION_H_