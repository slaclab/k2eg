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
  return received_event->event_data->size() > 0;
}

bool
MonitorOperationImpl::hasEvents() const {
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
    : monitor_principal_request(MakeMonitorOperationImplUPtr(channel, pv_name, principal_request)),
      monitor_additional_request(MakeMonitorOperationImplUPtr(channel, pv_name, additional_request)),
      structure_merger(std::make_unique<PVStructureMerger>()),
      evt_received(std::make_shared<EventReceived>()) {}

EventReceivedShrdPtr
CombinedMonitorOperation::getEventData() const {
  std::lock_guard<std::mutex> l(evt_mtx);
  // wait to receive data from the two monitor
  EventReceivedShrdPtr joined_evt = std::make_shared<EventReceived>();
  if (!monitor_principal_request->hasEvents() && (!monitor_additional_request->hasData() && !last_additional_evt_received)) return joined_evt;
  auto a_evt_received = monitor_principal_request->getEventData();
  // get last event from additional data
  if (monitor_additional_request->hasData()) {
    // in this case if principal request has not produced data i put the last one received
    if (!a_evt_received->event_data->size()) { a_evt_received->event_data->push_back(last_additional_evt_received); }
    // get received additional data and take the only last
    auto add_evt_data            = monitor_additional_request->getEventData();
    last_additional_evt_received = add_evt_data->event_data->at(add_evt_data->event_data->size() - 1);
  }

  // copy all other event except data from the first request
  joined_evt->event_cancel     = a_evt_received->event_cancel;
  joined_evt->event_disconnect = a_evt_received->event_disconnect;
  joined_evt->event_fail       = a_evt_received->event_fail;
  // merge all data from principal request to the last event
  for (auto& a_data : *a_evt_received->event_data) {
    // join all the event data from the principal request, with the last from additional request
    // event from principal are more important than from additional one request
    auto merge_event_data = structure_merger->mergeStructureAndValue({a_data->channel_data.data, last_additional_evt_received->channel_data.data});
    joined_evt->event_data->push_back(MakeMonitorEventShrdPtr(a_data->type, "", ChannelData(a_data->channel_data.pv_name, merge_event_data)));
    last_principal_evt_received = a_data;
  }
  return joined_evt;
}

bool
CombinedMonitorOperation::hasData() const {
  return monitor_principal_request->hasData() && (monitor_principal_request->hasData() || last_additional_evt_received);
}

bool
CombinedMonitorOperation::hasEvents() const {
  return monitor_principal_request->hasEvents() && (monitor_principal_request->hasData() || last_additional_evt_received);
}

const std::string&
CombinedMonitorOperation::getPVName() const {
  return monitor_principal_request->getPVName();
}