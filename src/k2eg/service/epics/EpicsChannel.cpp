#include <k2eg/service/epics/EpicsChannel.h>

#include <pv/caProvider.h>
#include <pv/clientFactory.h>

#include <memory>

using namespace k2eg::service::epics_impl;

namespace pva = epics::pvAccess;

EpicsChannel::EpicsChannel(pvac::ClientProvider& provider, const std::string& pv_name, const std::string& address) : pv_name(pv_name), address(address) {
  pvac::ClientChannel::Options opt;
  if (!address.empty()) { opt.address = address; }
  channel = std::make_shared<pvac::ClientChannel>(provider.connect(pv_name, opt));
}

EpicsChannel::~EpicsChannel() {}

void
EpicsChannel::init() {
  // "pva" provider automatically in registry
  // add "ca" provider to registry
  pva::ca::CAClientFactory::start();
}

void
EpicsChannel::deinit() {
  pva::ca::CAClientFactory::stop();
}

ConstPutOperationUPtr
EpicsChannel::put(const std::string& field, const std::string& value) {
  return MakePutOperationUPtr(channel, pvReq, field, value);
}

ConstGetOperationUPtr
EpicsChannel::get(const std::string& field, const std::string& additional_filed) const {
  if (additional_filed.empty())
    return MakeSingleGetOperationUPtr(channel, pv_name, field);
  else
    return MakeCombinedGetOperationUPtr(MakeSingleGetOperationShrdPtr(channel, pv_name, field),
                                        MakeSingleGetOperationShrdPtr(channel, pv_name, additional_filed));
}

ConstMonitorOperationShrdPtr
EpicsChannel::monitor(const std::string& fastUpdateField, const std::string& slowField ) const {
  return std::make_shared<MonitorOperationImpl>(channel, pv_name, fastUpdateField);
}