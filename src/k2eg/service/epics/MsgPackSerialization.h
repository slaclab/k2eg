#ifndef K2EG_SERVICE_EPICS_MSGPACKSERIALIZATION_H_
#define K2EG_SERVICE_EPICS_MSGPACKSERIALIZATION_H_

#include <k2eg/service/epics/Serialization.h>

#include <pvData.h>

#include <msgpack.hpp>

namespace k2eg::service::epics_impl {
// JSON serializer
class MsgPackSerializer : public Serializer
{
    // void processScalar(const epics::pvData::PVScalar* scalar, msgpack::packer<msgpack::sbuffer>& packer);
    // void processScalarArray(const epics::pvData::PVScalarArray* scalarArray, msgpack::packer<msgpack::sbuffer>& packer);
    // void processStructure(const epics::pvData::PVStructure* scalarArray, msgpack::packer<msgpack::sbuffer>& packer);
    // void processStructureArray(epics::pvData::PVStructureArray::const_svector structure_array, msgpack::packer<msgpack::sbuffer>& packer);
    // void processUnion(const epics::pvData::PVUnion* union_, msgpack::packer<msgpack::sbuffer>& packer);
    // void processUnionArray(const epics::pvData::PVUnionArray::const_svector union_array, msgpack::packer<msgpack::sbuffer>& packer);

public:
    MsgPackSerializer() = default;
    virtual ~MsgPackSerializer() = default;
    void serialize(const ChannelData& message, common::SerializedMessage& serialized_message);
    k2eg::common::SerializedMessageShrdPtr serialize(const ChannelData& message, const std::string& reply_id = "");
};
DEFINE_PTR_TYPES(MsgPackSerializer)
// Serialization message for json encoding

} // namespace k2eg::service::epics_impl

#endif // K2EG_SERVICE_EPICS_MSGPACKSERIALIZATION_H_