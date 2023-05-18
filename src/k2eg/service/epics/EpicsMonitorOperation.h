#ifndef K2EG_SERVICE_EPICS_EPICSMONITOROPERATION_H_
#define K2EG_SERVICE_EPICS_EPICSMONITOROPERATION_H_

#include <k2eg/common/types.h>
#include <k2eg/service/epics/EpicsData.h>
#include <k2eg/service/epics/PVStructureMerger.h>
#include <pvData.h>
#include <pva/client.h>

#include <mutex>

namespace k2eg::service::epics_impl {
class MonitorOperationImpl;
class CombinedMonitorOperation;
// abstract monitor operation
class MonitorOperation {
  friend class MonitorOperationImpl;
  friend class CombinedMonitorOperation;

  MonitorOperation() = default;

 public:
  virtual ~MonitorOperation()                       = default;
  virtual EventReceivedShrdPtr getEventData() const = 0;
  virtual bool                 hasData() const      = 0;
  virtual bool                 hasEvents() const    = 0;
  virtual const std::string&   getPVName() const    = 0;
};
DEFINE_PTR_TYPES(MonitorOperation)

// async monitor operation ofr a single set of field
class MonitorOperationImpl : public pvac::ClientChannel::MonitorCallback, public MonitorOperation {
  const std::string                    field;
  const std::string                    pv_name;
  pvac::Monitor                        mon;
  std::shared_ptr<pvac::ClientChannel> channel;
  mutable EventReceivedShrdPtr         received_event;
  mutable std::mutex                   ce_mtx;

 public:
  MonitorOperationImpl(std::shared_ptr<pvac::ClientChannel> channel, const std::string& pv_name, const std::string& field = "field()");
  virtual ~MonitorOperationImpl();

  virtual void         monitorEvent(const pvac::MonitorEvent& evt) OVERRIDE FINAL;
  EventReceivedShrdPtr getEventData() const OVERRIDE FINAL;
  bool                 hasData() const OVERRIDE FINAL;
  bool                 hasEvents() const OVERRIDE FINAL;
  const std::string&   getPVName() const OVERRIDE FINAL;
};

DEFINE_PTR_TYPES(MonitorOperationImpl)

// combine two async monitor operation together
class CombinedMonitorOperation : public MonitorOperation {
  MonitorOperationImplUPtr monitor_principal_request;
  mutable bool             structure_a_received;
  MonitorOperationImplUPtr monitor_additional_request;
  mutable bool             structure_b_received;
  PVStructureMergerUPtr    structure_merger;
  EventReceivedShrdPtr     evt_received;
  mutable std::mutex       evt_mtx;

 public:
  CombinedMonitorOperation(std::shared_ptr<pvac::ClientChannel> channel,
                           const std::string&                   pv_name,
                           const std::string&                   principal_request,
                           const std::string&                   additional_request);
  virtual ~CombinedMonitorOperation() = default;
  EventReceivedShrdPtr getEventData() const OVERRIDE FINAL;
  bool                 hasData() const OVERRIDE FINAL;
  bool                 hasEvents() const OVERRIDE FINAL;
  const std::string&   getPVName() const OVERRIDE FINAL;
};
DEFINE_PTR_TYPES(CombinedMonitorOperation)

}  // namespace k2eg::service::epics_impl

#endif  // K2EG_SERVICE_EPICS_EPICSMONITOROPERATION_H_