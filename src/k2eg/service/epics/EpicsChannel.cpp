#include <k2eg/service/epics/EpicsChannel.h>
#include <pv/caProvider.h>
#include <pv/clientFactory.h>

#include <memory>

#include <k2eg/service/epics/EpicsMonitorOperation.h>

using namespace k2eg::service::epics_impl;

namespace pva = epics::pvAccess;

EpicsChannel::EpicsChannel(pvac::ClientProvider& provider, const std::string& pv_name, const std::string& address)
    : pv_name(pv_name)
    , address(address)
    , fetch_principal_field(provider.name().compare("ca") == 0 ? "field(value," "timeStamp," "alarm)" : "field()")
    , fetch_additional_field(provider.name().compare("ca") == 0 ? "field(" "dis" "pla" "y," "control," "valueAlarm)" : "")
{
    pvac::ClientChannel::Options opt;
    if (!address.empty())
    {
        opt.address = address;
    }
    channel = std::make_shared<pvac::ClientChannel>(provider.connect(pv_name, opt));
}

EpicsChannel::~EpicsChannel() {}

void EpicsChannel::init()
{
    // "pva" provider automatically in registry
    // add "ca" provider to registry
    pva::ca::CAClientFactory::start();
}

void EpicsChannel::deinit()
{
    pva::ca::CAClientFactory::stop();
}

ConstPutOperationUPtr EpicsChannel::put(const std::string& field, const std::string& value)
{
    return MakePutOperationUPtr(channel, pvReq, field, value);
}

ConstGetOperationUPtr EpicsChannel::get() const
{
    if (fetch_additional_field.empty())
        return MakeSingleGetOperationUPtr(channel, pv_name, fetch_principal_field);
    else
        return MakeCombinedGetOperationUPtr(MakeSingleGetOperationShrdPtr(channel, pv_name, fetch_principal_field), MakeSingleGetOperationShrdPtr(channel, pv_name, fetch_additional_field));
}

ConstMonitorOperationShrdPtr EpicsChannel::monitor() const
{
    ConstMonitorOperationShrdPtr result;
    if (fetch_additional_field.empty())
    {
        result = MakeMonitorOperationImplShrdPtr(channel, pv_name, fetch_principal_field);
    }
    else
    {
        result = MakeCombinedMonitorOperationShrdPtr(channel, pv_name, fetch_principal_field, fetch_additional_field);
    }
    return result;
}