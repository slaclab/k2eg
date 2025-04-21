#ifndef K2EG_SERVICE_EPICS_EPICSDATA_H_
#define K2EG_SERVICE_EPICS_EPICSDATA_H_

#include <k2eg/common/ObjectFactory.h>
#include <k2eg/common/types.h>
#include <pv/pvData.h>

#include <memory>

namespace k2eg::service::epics_impl {

// Epics Channel Data
struct ChannelData
{
    const std::string                                pv_name;
    epics::pvData::PVStructure::const_shared_pointer data;
};
DEFINE_PTR_TYPES(ChannelData)

// event type
enum EventType
{
    Timeout,
    Fail,
    Cancel,
    Disconnec,
    Data
};

typedef struct
{
    EventType         type;
    const std::string message;
    ChannelData       channel_data;
} MonitorEvent;
DEFINE_PTR_TYPES(MonitorEvent)

typedef std::vector<MonitorEventShrdPtr> MonitorEventVec;
typedef std::shared_ptr<MonitorEventVec> MonitorEventVecShrdPtr;

struct EventReceived
{
    MonitorEventVecShrdPtr event_timeout = std::make_shared<MonitorEventVec>();
    MonitorEventVecShrdPtr event_data = std::make_shared<MonitorEventVec>();
    MonitorEventVecShrdPtr event_fail = std::make_shared<MonitorEventVec>();
    MonitorEventVecShrdPtr event_disconnect = std::make_shared<MonitorEventVec>();
    MonitorEventVecShrdPtr event_cancel = std::make_shared<MonitorEventVec>();
};
DEFINE_PTR_TYPES(EventReceived)

} // namespace k2eg::service::epics_impl

#endif // K2EG_SERVICE_EPICS_EPICSDATA_H_