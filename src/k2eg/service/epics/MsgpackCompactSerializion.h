#ifndef K2EG_SERVICE_EPICS_MSGPACKCOMPACTSERIALIZER_H_
#define K2EG_SERVICE_EPICS_MSGPACKCOMPACTSERIALIZER_H_

#include <k2eg/service/epics/Serialization.h>
#include <msgpack.hpp>

namespace k2eg::service::epics_impl {

class MsgpackCompactSerializer : public Serializer {
    void processScalar(const epics::pvData::PVScalar* scalar, msgpack::packer<msgpack::sbuffer>& packer);
    void processScalarArray(const epics::pvData::PVScalarArray* scalarArray, msgpack::packer<msgpack::sbuffer>& packer);
    void scannStructure(const epics::pvData::PVStructure* scalarArray, std::vector<const epics::pvData::PVField*>& values);
    void scannStructureArray(epics::pvData::PVStructureArray::const_svector structure_array, std::vector<const epics::pvData::PVField*>& values);
public:
    MsgpackCompactSerializer() = default;
    virtual ~MsgpackCompactSerializer() = default;
    SerializedMessageShrdPtr serialize(const ChannelData& message);
};
DEFINE_PTR_TYPES(MsgpackCompactSerializer)

// Message for msgpack compact encoding
class MsgpackCompactMessage : public SerializedMessage {
    friend class MsgpackCompactSerializer;
    msgpack::sbuffer buf;
    epics::pvData::PVStructure::const_shared_pointer epics_pv_struct;
public:
    MsgpackCompactMessage(epics::pvData::PVStructure::const_shared_pointer epics_pv_struct);
    MsgpackCompactMessage() = default;
    ~MsgpackCompactMessage() = default;
    const size_t size() const;
    const char* data() const;
};
DEFINE_PTR_TYPES(MsgpackCompactMessage)
}
#endif  // K2EG_SERVICE_EPICS_MSGPACKCOMPACTSERIALIZER_H_