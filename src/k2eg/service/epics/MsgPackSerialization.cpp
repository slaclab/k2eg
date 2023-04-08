#include <k2eg/service/epics/MsgPackSerialization.h>

#include <pv/bitSet.h>
#include <sstream>

using namespace k2eg::service::epics_impl;

namespace pvd = epics::pvData;

#pragma region MsgPackMessage
MsgPackMessage::MsgPackMessage(epics::pvData::PVStructure::const_shared_pointer):
epics_pvstructure(std::move(epics_pvstructure)) {}
const size_t MsgPackMessage::size() const { return buf.size(); }
const char* MsgPackMessage::data() const { return buf.data(); }
#pragma endregion MsgPackMessage

#pragma region MsgPackSerializer
REGISTER_SERIALIZER(SerializationType::MsgPack, MsgPackSerializer)
ConstSerializedMessageUPtr MsgPackSerializer::serialize(const ChannelData& message) {
    auto result = MakeMsgPackMessageUPtr(std::move(message.data));
    msgpack::packer<msgpack::sbuffer> packer(&result->buf);
    // add channel message
    packer.pack_map(1);
    packer.pack(std::string_view(message.channel_name));
    // process root structure
    processStructure(message.data.get(), packer);
    return std::move(result);
}

void MsgPackSerializer::processScalar(const pvd::PVScalar* scalar, msgpack::packer<msgpack::sbuffer>& packer) {
    switch (scalar->getScalar()->getScalarType()) {
    case pvd::ScalarType::pvBoolean: {
        packer.pack(scalar->getAs<pvd::boolean>());
        break;
    }
    case pvd::ScalarType::pvByte: {
        packer.pack(scalar->getAs<pvd::int8>());
        break;
    }
    case pvd::ScalarType::pvDouble: {
        packer.pack(scalar->getAs<double>());
        break;
    }
    case pvd::ScalarType::pvFloat: {
        packer.pack(scalar->getAs<float>());
        break;
    }
    case pvd::ScalarType::pvInt: {
        packer.pack(scalar->getAs<pvd::int32>());
        break;
    }
    case pvd::ScalarType::pvLong: {
        packer.pack(scalar->getAs<pvd::int64>());
        break;
    }
    case pvd::ScalarType::pvShort: {
        packer.pack(scalar->getAs<pvd::int16>());
        break;
    }
    case pvd::ScalarType::pvString: {
        packer.pack(scalar->getAs<std::string>());
        break;
    }
    case pvd::ScalarType::pvUByte: {
        packer.pack(scalar->getAs<pvd::uint8>());
        break;
    }
    case pvd::ScalarType::pvUInt: {
        packer.pack(scalar->getAs<pvd::uint32>());
        break;
    }
    case pvd::ScalarType::pvULong: {
        packer.pack(scalar->getAs<pvd::uint64>());
        break;
    }
    case pvd::ScalarType::pvUShort: {
        packer.pack(scalar->getAs<pvd::uint16>());
        break;
    }
    }
}

void MsgPackSerializer::processScalarArray(const pvd::PVScalarArray* scalarArray, msgpack::packer<msgpack::sbuffer>& packer) {
    pvd::shared_vector<const void> arr;
    scalarArray->getAs<const void>(arr);
    packer.pack_bin(arr.size());
    packer.pack_bin_body(static_cast<const char *>(arr.data()), arr.size());
    //switch (scalarArray->getScalarArray()->getElementType()) {
    // case pvd::ScalarType::pvBoolean: {
    //     packer.pack(pvd::shared_vector_convert<const pvd::boolean>(arr));
        
    //     break;
    // }
    // case pvd::ScalarType::pvByte: {
    //     packer.pack(pvd::shared_vector_convert<const pvd::int8>(arr));
    //     break;
    // }
    // case pvd::ScalarType::pvDouble: {
    //     packer.pack(pvd::shared_vector_convert<const double>(arr));
    //     break;
    // }
    // case pvd::ScalarType::pvFloat: {
    //     packer.pack(pvd::shared_vector_convert<const float>(arr));
    //     break;
    // }
    // case pvd::ScalarType::pvInt: {
    //     packer.pack(pvd::shared_vector_convert<const pvd::int32>(arr));
    //     break;
    // }
    // case pvd::ScalarType::pvLong: {
    //     packer.pack(pvd::shared_vector_convert<const pvd::int64>(arr));
    //     break;
    // }
    // case pvd::ScalarType::pvShort: {
    //     packer.pack(pvd::shared_vector_convert<const pvd::int8>(arr));
    //     break;
    // }
    // case pvd::ScalarType::pvString: {
    //     packer.pack(pvd::shared_vector_convert<const std::string>(arr));
    //     break;
    // }
    // case pvd::ScalarType::pvUByte: {
    //     packer.pack(pvd::shared_vector_convert<const pvd::uint8>(arr));
    //     break;
    // }
    // case pvd::ScalarType::pvUInt: {
    //     packer.pack(pvd::shared_vector_convert<const pvd::uint32>(arr));
    //     break;
    // }
    // case pvd::ScalarType::pvULong: {
    //     packer.pack(pvd::shared_vector_convert<const pvd::uint64>(arr));
    //     break;
    // }
    // case pvd::ScalarType::pvUShort: {
    //     packer.pack(pvd::shared_vector_convert<const pvd::uint16>(arr));
    //     break;
    // }
    //}
}

void MsgPackSerializer::processStructure(const epics::pvData::PVStructure* structure, msgpack::packer<msgpack::sbuffer>& packer) {
    const pvd::StructureConstPtr& type = structure->getStructure();
    const pvd::PVFieldPtrArray& children = structure->getPVFields();
    const pvd::StringArray& names = type->getFieldNames();

    // init map
    packer.pack_map(names.size());

    for (size_t i = 0, N = names.size(); i < N; i++) {
        auto const& fld = children[i].get();

        // pack key
        packer.pack(std::string_view(names[i]));
        auto type = fld->getField()->getType();
        switch (type) {
        case pvd::Type::scalar: {
            processScalar(static_cast<const pvd::PVScalar*>(fld), packer);
            break;
        }
        case pvd::Type::scalarArray: {
            processScalarArray(static_cast<const pvd::PVScalarArray*>(fld), packer);
            break;
        }
        case pvd::Type::structure: {
            processStructure(static_cast<const pvd::PVStructure*>(fld), packer);
            break;
        }
        }
    }
}

#pragma endregion MsgPackSerializer