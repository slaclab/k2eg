#include <k2eg/service/epics/EpicsChannel.h>
#include <pv/caProvider.h>
#include <pv/clientFactory.h>
#include <memory>
#include "k2eg/service/epics/EpicsGetOperation.h"
#include "k2eg/service/epics/EpicsPutOperation.h"

using namespace k2eg::service::epics_impl;

namespace pvd = epics::pvData;
namespace pva = epics::pvAccess;

EpicsChannel::EpicsChannel(pvac::ClientProvider& provider, const std::string& pv_name, const std::string& address)
    : pv_name(pv_name), address(address) {
  pvac::ClientChannel::Options opt;
  if (!address.empty()) { opt.address = address; }
  channel = std::make_shared<pvac::ClientChannel>(provider.connect(pv_name, opt));
}

EpicsChannel::~EpicsChannel() {
  //if (channel) { channel->reset(); }
  //if (provider) { provider->disconnect(); }
}

void
EpicsChannel::init() {
  // "pva" provider automatically in registry
  // add "ca" provider to registry
  pva::ca::CAClientFactory::start();
}

void
EpicsChannel::deinit() {
  // "pva" provider automatically in registry
  // add "ca" provider to registry
  pva::ca::CAClientFactory::stop();
}

ConstPutOperationUPtr
EpicsChannel::put(const std::string& field, const std::string& value) {
  return MakePutOperationUPtr(channel, pvReq, field, value);
}

ConstGetOperationUPtr
EpicsChannel::get(const std::string& field) const {
  return MakeGetOperationUPtr(channel, pv_name, field);
}

void
EpicsChannel::startMonitor(const std::string& field) {
  mon = channel->monitor(pvd::createRequest(field));
}

EventReceivedShrdPtr
EpicsChannel::monitor() {
  EventReceivedShrdPtr result = std::make_shared<EventReceived>();
  if (!mon.wait(0.100)) {
    // updates mon.event
    result->event_timeout->push_back(std::make_shared<MonitorEvent>(MonitorEvent{EventType::Timeout, pv_name, "Time out", nullptr}));
    return result;
  }

  switch (mon.event.event) {
    // Subscription network/internal error
    case pvac::MonitorEvent::Fail:
      result->event_fail->push_back(std::make_shared<MonitorEvent>(MonitorEvent{EventType::Fail, pv_name, mon.event.message, nullptr}));
      break;
    // explicit call of 'mon.cancel' or subscription dropped
    case pvac::MonitorEvent::Cancel:
      result->event_cancel->push_back(std::make_shared<MonitorEvent>(MonitorEvent{EventType::Cancel, pv_name, mon.event.message, nullptr}));
      break;
    // Underlying channel becomes disconnected
    case pvac::MonitorEvent::Disconnect:
      result->event_disconnect->push_back(std::make_shared<MonitorEvent>(MonitorEvent{EventType::Disconnec, pv_name, mon.event.message, nullptr}));
      break;
    // Data queue becomes not-empty
    case pvac::MonitorEvent::Data:
      // We drain event FIFO completely
      while (mon.poll()) {
        auto tmp_data = std::make_shared<epics::pvData::PVStructure>(mon.root->getStructure());
        tmp_data->copy(*mon.root);
        result->event_data->push_back(std::make_shared<MonitorEvent>(MonitorEvent{EventType::Data, mon.event.message, {pv_name, tmp_data}}));
      }
      break;
  }
  return result;
}

void
EpicsChannel::stopMonitor() {
  if (mon) { mon.cancel(); }
}