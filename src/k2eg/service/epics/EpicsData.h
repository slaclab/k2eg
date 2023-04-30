#ifndef K2EG_SERVICE_EPICS_EPICSDATA_H_
#define K2EG_SERVICE_EPICS_EPICSDATA_H_

#include <k2eg/common/ObjectFactory.h>
#include <k2eg/common/types.h>
#include <pv/pvData.h>

#include <memory>
#include <set>
namespace k2eg::service::epics_impl {

// Epics Channel Data
struct ChannelData {
    const std::string pv_name;
    epics::pvData::PVStructure::const_shared_pointer data;
};
DEFINE_PTR_TYPES(ChannelData)

} // namespace k2eg::service::epics_impl

#endif // K2EG_SERVICE_EPICS_EPICSDATA_H_