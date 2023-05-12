#include <k2eg/service/epics/EpicsMonitorOperation.h>
#include <mutex>
#include "k2eg/service/epics/EpicsData.h"

using namespace k2eg::service::epics_impl;

MonitorOperation::MonitorOperation(std::shared_ptr<pvac::ClientChannel> channel, const std::string& pv_name, const std::string& field)
    : channel(channel), pv_name(pv_name), field(field) {}

MonitorOperation::~MonitorOperation() { mon.cancel(); }

void
MonitorOperation::monitorEvent(const pvac::MonitorEvent& evt) {
  EventReceivedShrdPtr result = std::make_shared<EventReceived>();
  // shared_from_this() will fail as Cancel is delivered in our dtor.
  if (evt.event == pvac::MonitorEvent::Cancel) return;
  // running on internal provider worker thread
  // minimize work here.
  switch (evt.event) {
    // case pvac::MonitorEvent::Fail: std::cout << "Error " << name << " " << evt.message << "\n"; break;
    // case pvac::MonitorEvent::Cancel: std::cout << "Cancel " << name << "\n"; break;
    // case pvac::MonitorEvent::Disconnect: std::cout << "Disconnect " << name << "\n"; break;
    case pvac::MonitorEvent::Data: {
    } break;
  }
}

EventReceivedShrdPtr
MonitorOperation::getEventData() const {
    std::lock_guard<std::mutex> l(ce_mtx);
    auto resutl = consumed_event;
    //consumed_event = std::make_shared<EventReceived>();
    return resutl;
}

bool
MonitorOperation::hasData() const {
    std::lock_guard<std::mutex> l(ce_mtx);
    return consumed_event->event_data->size() > 0;
}