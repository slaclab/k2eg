#include <k2eg/service/epics/MsgPackSerialization.h>

#include <pv/bitSet.h>
#include <sstream>

using namespace k2eg::service::epics_impl;

using namespace msgpack;
namespace pvd = epics::pvData;

#pragma region JsonMessage
MsgPackMessage::MsgPackMessage(msgpack::sbuffer msgpack_buffer)
    : msgpack_buffer(std::move(msgpack_buffer)) {}

const size_t MsgPackMessage::size() const { return msgpack_buffer.size(); }
const char* MsgPackMessage::data() const { return msgpack_buffer.data(); }
#pragma endregion JsonMessage

#pragma region JsonSerializer
REGISTER_SERIALIZER(SerializationType::MessagePack, MsgPackSerializer)
ConstSerializedMessageUPtr MsgPackSerializer::serialize(const ChannelData& message) {
    sbuffer sbuf;
    msgpack::packer<msgpack::sbuffer> packer(&sbuf);

    const pvd::StructureConstPtr& type = message.data->getStructure();
    const pvd::PVFieldPtrArray& children = message.data->getPVFields();
    const pvd::StringArray& names = type->getFieldNames();

    // init map
    packer.pack_map(names.size());

    for (size_t i = 0, N = names.size(); i < N; i++) {
        auto const& fld = children[i].get();

        // pack key
        packer.pack(std::string_view(names[i]));

        switch (fld->getField()->getType()) {
        case pvd::Type::scalar: {
            processScalar(static_cast<const pvd::PVScalar*>(fld), packer);
            break;
        }
        case pvd::Type::scalarArray: {
            processScalarArray(static_cast<const pvd::PVScalarArray*>(fld), packer);
            break;
        }
        }
    }

    // std::tuple<int, std::string_view, msgpack::type::raw_ref> data(idx, "example", msgpack::type::raw_ref(buffer.get(), buffer_size));
    // packer.pack_map std::cout << "Index " << idx << "\t\r" << std::flush;
    return nullptr;
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
#pragma endregion JsonSerializer