#ifndef K2EG_SERVICE_EPICS_MSGPACKSERIALIZATION_H_
#define K2EG_SERVICE_EPICS_MSGPACKSERIALIZATION_H_

#include <k2eg/service/epics/Serialization.h>
#include <msgpack.hpp>

namespace k2eg::service::epics_impl {
// JSON serializer
class MsgPackSerializer : public Serializer {
    void processScalar(const epics::pvData::PVScalar* scalar, msgpack::packer<msgpack::sbuffer>& packer);
    void processScalarArray(const epics::pvData::PVScalarArray* scalarArray, msgpack::packer<msgpack::sbuffer>& packer);
    void processStructure(const epics::pvData::PVStructure* scalarArray, msgpack::packer<msgpack::sbuffer>& packer);
    void processStructureArray(epics::pvData::PVStructureArray::const_svector structure_array, msgpack::packer<msgpack::sbuffer>& packer);
public:
    MsgPackSerializer() = default;
    virtual ~MsgPackSerializer() = default;
    k2eg::common::SerializedMessageShrdPtr serialize(const ChannelData& message, const std::string& reply_id = "");
};
DEFINE_PTR_TYPES(MsgPackSerializer)
// Serialization message for json encoding
class MsgPackMessage : public k2eg::common::SerializedMessage {
    friend class MsgPackSerializer;
    msgpack::sbuffer buf;
    epics::pvData::PVStructure::const_shared_pointer epics_pv_struct;
public:
    MsgPackMessage(epics::pvData::PVStructure::const_shared_pointer epics_pv_struct);
    MsgPackMessage() = default;
    ~MsgPackMessage() = default;
    const size_t size() const;
    const char* data() const;
};
DEFINE_PTR_TYPES(MsgPackMessage)
} // namespace k2eg::service::epics_impl

#endif // K2EG_SERVICE_EPICS_MSGPACKSERIALIZATION_H_