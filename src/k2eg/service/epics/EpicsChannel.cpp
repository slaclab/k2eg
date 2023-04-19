#include <k2eg/service/epics/EpicsChannel.h>
#include <pv/caProvider.h>
#include <pv/clientFactory.h>
#include "k2eg/service/epics/EpicsPutOperation.h"

using namespace k2eg::service::epics_impl;

namespace pvd = epics::pvData;
namespace pva = epics::pvAccess;

EpicsChannel::EpicsChannel(const std::string& provider_name, const std::string& channel_name, const std::string& address)
    : channel_name(channel_name), address(address) {
  provider = std::make_unique<pvac::ClientProvider>(provider_name, conf);
}

EpicsChannel::~EpicsChannel() {
  if (channel) { channel->reset(); }
  if (provider) { provider->disconnect(); }
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

void
EpicsChannel::connect() {
  pvac::ClientChannel::Options opt;
  if (!address.empty()) { opt.address = address; }
  channel = std::make_unique<pvac::ClientChannel>(provider->connect(channel_name, opt));
}

ConstChannelDataUPtr
EpicsChannel::getChannelData() const {
  ConstChannelDataUPtr result;
  try {
    result = std::make_unique<ChannelData>(ChannelData{channel_name, channel->get()});
  } catch (pvac::Timeout to) {
    // ltimeout error
  }
  return result;
}

pvd::PVStructure::const_shared_pointer
EpicsChannel::getData() const {
  return channel->get();
}

void
EpicsChannel::putData(const std::string& name, const epics::pvData::AnyScalar& new_value) const {
  channel->put().set(name, new_value).exec();
}

ConstPutOperationUPtr
EpicsChannel::putValue(const std::string& value) {
  return MakePutOperationUPtr(channel, pvReq, value);
}

void
EpicsChannel::startMonitor() {
  mon = channel->monitor();
}

MonitorEventVecShrdPtr
EpicsChannel::monitor() {
  auto result = std::make_shared<MonitorEventVec>();
  if (!mon.wait(0.100)) {
    // updates mon.event
    return result;
  }

  switch (mon.event.event) {
    // Subscription network/internal error
    case pvac::MonitorEvent::Fail:
      result->push_back(std::make_shared<MonitorEvent>(MonitorEvent{MonitorType::Fail, channel_name, mon.event.message, nullptr}));
      break;
    // explicit call of 'mon.cancel' or subscription dropped
    case pvac::MonitorEvent::Cancel:
      result->push_back(std::make_shared<MonitorEvent>(MonitorEvent{MonitorType::Cancel, channel_name, mon.event.message, nullptr}));
      break;
    // Underlying channel becomes disconnected
    case pvac::MonitorEvent::Disconnect:
      result->push_back(std::make_shared<MonitorEvent>(MonitorEvent{MonitorType::Disconnec, channel_name, mon.event.message, nullptr}));
      break;
    // Data queue becomes not-empty
    case pvac::MonitorEvent::Data:
      // We drain event FIFO completely
      while (mon.poll()) {
        auto tmp_data = std::make_shared<epics::pvData::PVStructure>(mon.root->getStructure());
        tmp_data->copy(*mon.root);
        result->push_back(std::make_shared<MonitorEvent>(MonitorEvent{MonitorType::Data, mon.event.message, {channel_name, tmp_data}}));
      }
      break;
  }
  return result;
}

void
EpicsChannel::stopMonitor() {
  if (mon) { mon.cancel(); }
}