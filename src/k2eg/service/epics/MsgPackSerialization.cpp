#include <k2eg/common/MsgpackSerialization.h>

#include <k2eg/service/epics/MsgPackSerialization.h>
#include <k2eg/service/epics/MsgpackEpicsConverter.h>

#include <k2eg/controller/command/cmd/Command.h>

#include <pv/bitSet.h>
#include <pvData.h>
#include <pvType.h>

#include <memory>

using namespace k2eg::service::epics_impl;
using namespace k2eg::common;
namespace pvd = epics::pvData;

void MsgPackSerializer::serialize(const ChannelData& message, SerializedMessage& serialized_message)
{
    MsgpackMessage&                   mp_msg = dynamic_cast<MsgpackMessage&>(serialized_message);
    msgpack::packer<msgpack::sbuffer> packer(mp_msg.getBuffer());
    packer.pack(message.pv_name);
    // process root structure using the new converter
    MsgpackEpicsConverter::epicsToMsgpack(std::const_pointer_cast<pvd::PVStructure>(message.data), packer);
}

REGISTER_SERIALIZER(SerializationType::Msgpack, MsgPackSerializer)

SerializedMessageShrdPtr MsgPackSerializer::serialize(const ChannelData& message, const std::string& reply_id)
{
    auto                              result = std::make_shared<MsgpackMessage>();
    msgpack::packer<msgpack::sbuffer> packer(result->getBuffer());
    if (reply_id.empty())
    {
        packer.pack_map(1);
        packer.pack(message.pv_name);
        // Use the new epicsToMsgpack method
        MsgpackEpicsConverter::epicsToMsgpack(std::const_pointer_cast<pvd::PVStructure>(message.data), packer);
    }
    else
    {
        packer.pack_map(2);
        packer.pack(KEY_REPLY_ID);
        packer.pack(reply_id);
        packer.pack(message.pv_name);
        MsgpackEpicsConverter::epicsToMsgpack(std::const_pointer_cast<pvd::PVStructure>(message.data), packer);
    }
    return result;
}