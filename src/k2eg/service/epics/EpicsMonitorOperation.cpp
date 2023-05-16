#include <k2eg/service/epics/EpicsMonitorOperation.h>
#include <pv/createRequest.h>

#include <memory>
#include <mutex>

#include "k2eg/service/epics/EpicsData.h"
#include "k2eg/service/epics/EpicsGetOperation.h"

using namespace k2eg::service::epics_impl;
namespace pvd = epics::pvData;

MonitorOperationImpl::MonitorOperationImpl(std::shared_ptr<pvac::ClientChannel> channel, const std::string& pv_name, const std::string& field)
    : channel(channel), pv_name(pv_name), field(field), received_event(std::make_shared<EventReceived>()) {
  mon = channel->monitor(this, pvd::createRequest(field));
}

MonitorOperationImpl::~MonitorOperationImpl() {
  if (mon) { mon.cancel(); }
}

void
MonitorOperationImpl::monitorEvent(const pvac::MonitorEvent& evt) {
  // running on internal provider worker thread
  // minimize work here.
  unsigned                    fetched = 0;
  std::lock_guard<std::mutex> l(ce_mtx);
  switch (evt.event) {
    // Subscription network/internal error
    case pvac::MonitorEvent::Fail:
      received_event->event_fail->push_back(std::make_shared<MonitorEvent>(MonitorEvent{EventType::Fail, pv_name, evt.message, nullptr}));
      break;
    // explicit call of 'mon.cancel' or subscription dropped
    case pvac::MonitorEvent::Cancel:
      // if (mon.valid()) {
      //   //mon is valid so we can continnue because this class is valid
      //   received_event->event_cancel->push_back(std::make_shared<MonitorEvent>(MonitorEvent{EventType::Cancel, pv_name, evt.message, nullptr}));
      // }
      break;
    // Underlying channel becomes disconnected
    case pvac::MonitorEvent::Disconnect:
      received_event->event_disconnect->push_back(std::make_shared<MonitorEvent>(MonitorEvent{EventType::Disconnec, pv_name, evt.message, nullptr}));
      break;
    // Data queue becomes not-empty
    case pvac::MonitorEvent::Data:
      // We drain event FIFO completely
      while (mon.poll()) {
        auto tmp_data = std::make_shared<epics::pvData::PVStructure>(mon.root->getStructure());
        tmp_data->copy(*mon.root);
        received_event->event_data->push_back(std::make_shared<MonitorEvent>(MonitorEvent{EventType::Data, evt.message, {pv_name, tmp_data}}));
      }
      break;
  }
}

EventReceivedShrdPtr
MonitorOperationImpl::getEventData() const {
  std::lock_guard<std::mutex> l(ce_mtx);
  auto                        resutl = received_event;
  received_event                     = std::make_shared<EventReceived>();
  return resutl;
}

bool
MonitorOperationImpl::hasData() const {
  std::lock_guard<std::mutex> l(ce_mtx);
  return received_event->event_data->size() > 0 || received_event->event_cancel->size() > 0 || received_event->event_disconnect->size() > 0 ||
         received_event->event_fail->size() > 0 || received_event->event_timeout->size() > 0;
}

const std::string&
MonitorOperationImpl::getPVName() const {
  return pv_name;
}

//----------------------------- CombinedMonitorOperation --------------------------------
CombinedMonitorOperation::CombinedMonitorOperation(std::shared_ptr<pvac::ClientChannel> channel,
                                                   const std::string&                   pv_name,
                                                   const std::string&                   principal_request,
                                                   const std::string&                   additional_request)
    : monitor_principal_request(MakeMonitorOperationImplUPtr(channel, pv_name, principal_request))
    ,monitor_additional_request(MakeMonitorOperationImplUPtr(channel, pv_name, additional_request)) {}

EventReceivedShrdPtr
CombinedMonitorOperation::getEventData() const {
  // TODO-make the structure recombinaiton from the two monitored structure
  return EventReceivedShrdPtr();
}
bool
CombinedMonitorOperation::hasData() const {
  return true;
}
const std::string&
CombinedMonitorOperation::getPVName() const {
  return monitor_principal_request->getPVName();
}