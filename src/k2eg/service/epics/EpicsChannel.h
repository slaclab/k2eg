#ifndef K2EG_SERVICE_EPICS_EPICSCHANNEL_H_
#define K2EG_SERVICE_EPICS_EPICSCHANNEL_H_

#include <k2eg/common/types.h>
#include <k2eg/common/MsgpackSerialization.h>

#include <k2eg/service/epics/EpicsData.h>
#include <k2eg/service/epics/EpicsPutOperation.h>
#include <k2eg/service/epics/EpicsGetOperation.h>
#include <k2eg/service/epics/EpicsMonitorOperation.h>

#include <cadef.h>
#include <pv/json.h>
#include <pva/client.h>
#include <pv/configuration.h>
#include <pv/createRequest.h>

#include <memory>
#include <string>

namespace k2eg::service::epics_impl {

class EpicsChannel
{
    friend class EpicsPutOperation;
    const std::string                          pv_name;
    const std::string                          address;
    const std::string                          fetch_principal_field;
    const std::string                          fetch_additional_field;
    epics::pvData::PVStructure::shared_pointer pvReq = epics::pvData::createRequest("field()");
    std::shared_ptr<pvac::ClientChannel>       channel;
    pvac::MonitorSync                          mon;

public:
    explicit EpicsChannel(pvac::ClientProvider& provider, const std::string& pv_name, const std::string& address = std::string());
    ~EpicsChannel();
    static void                  init();
    static void                  deinit();
    ConstPutOperationUPtr        put(const std::string& field, std::unique_ptr<k2eg::common::MsgpackObjectWithZone> value);
    ConstGetOperationUPtr        get() const;
    ConstMonitorOperationShrdPtr monitor() const;
};

DEFINE_PTR_TYPES(EpicsChannel)

} // namespace k2eg::service::epics_impl
#endif
