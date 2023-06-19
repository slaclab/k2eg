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
    void serialize(const ChannelData& message, k2eg::common::SerializedMessage& serialized_message);
    k2eg::common::SerializedMessageShrdPtr serialize(const ChannelData& message, const std::string& reply_id = "");
};
DEFINE_PTR_TYPES(MsgpackCompactSerializer)
}
#endif  // K2EG_SERVICE_EPICS_MSGPACKCOMPACTSERIALIZER_H_