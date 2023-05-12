#include <k2eg/service/epics/EpicsMonitorOperation.h>
#include <pv/createRequest.h>

#include <memory>
#include <mutex>

#include "k2eg/service/epics/EpicsData.h"

using namespace k2eg::service::epics_impl;
namespace pvd = epics::pvData;

MonitorOperation::MonitorOperation(std::shared_ptr<pvac::ClientChannel> channel, const std::string& pv_name, const std::string& field)
    : channel(channel), pv_name(pv_name), field(field), received_event(std::make_shared<EventReceived>()) {
  channel->addConnectListener(this);
}

MonitorOperation::~MonitorOperation() {
  if (mon) { mon.cancel(); }
  channel->removeConnectListener(this);
}

void
MonitorOperation::connectEvent(const pvac::ConnectEvent& evt) {
  if (evt.connected) { mon = channel->monitor(this, pvd::createRequest(field)); }
}

void
MonitorOperation::monitorEvent(const pvac::MonitorEvent& evt) {
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
      received_event->event_cancel->push_back(std::make_shared<MonitorEvent>(MonitorEvent{EventType::Cancel, pv_name, evt.message, nullptr}));
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
MonitorOperation::getEventData() const {
  std::lock_guard<std::mutex> l(ce_mtx);
  auto                        resutl = received_event;
  received_event                     = std::make_shared<EventReceived>();
  return resutl;
}

bool
MonitorOperation::hasData() const {
  std::lock_guard<std::mutex> l(ce_mtx);
  return received_event->event_data->size() > 0 || received_event->event_cancel->size() > 0 || received_event->event_disconnect->size() > 0 ||
         received_event->event_fail->size() > 0 || received_event->event_timeout->size() > 0;
}

const std::string&
MonitorOperation::getPVName() const {
  return pv_name;
}