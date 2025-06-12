#ifndef K2EG_SERVICE_EPICS_MSGPACKEPICSCONVERTER_H_
#define K2EG_SERVICE_EPICS_MSGPACKEPICSCONVERTER_H_

#include <msgpack.hpp>
#include <pv/pvData.h>

namespace k2eg::service::epics_impl {
class MsgpackEpicsConverter
{
    static void packField(msgpack::packer<msgpack::sbuffer>& pk, const epics::pvData::PVFieldPtr& field);
    static epics::pvData::PVFieldPtr unpackMsgpackToPVField(const msgpack::object& obj, const epics::pvData::FieldConstPtr& field);

public:
    static void epicsToMsgpack(const epics::pvData::PVStructurePtr& pvStruct, msgpack::packer<msgpack::sbuffer>& pk);
    static epics::pvData::PVStructurePtr msgpackToEpics(const msgpack::object& obj, const epics::pvData::StructureConstPtr& schema);
};
} // namespace k2eg::service::epics_impl

#endif // K2EG_SERVICE_EPICS_MSGPACKEPICSCONVERTER_H_