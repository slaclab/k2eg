#include <k2eg/common/MsgpackSerialization.h>

#include <k2eg/service/epics/MsgpackCompactSerializion.h>

#include <k2eg/controller/command/cmd/Command.h>

#include <memory>
#include <vector>
#include <pvType.h>

using namespace k2eg::service::epics_impl;
using namespace k2eg::common;
namespace pvd = epics::pvData;

void MsgpackCompactSerializer::serialize(const ChannelData& message, SerializedMessage& serialized_message)
{
    std::vector<const pvd::PVField*>  values;
    MsgpackMessage&                   mp_msg = dynamic_cast<MsgpackMessage&>(serialized_message);
    msgpack::packer<msgpack::sbuffer> packer(mp_msg.getBuffer());
    scannStructure(message.data.get(), values);
    packer.pack(message.pv_name);
    packer.pack_array(values.size());
    // serialize
    for (auto& f : values)
    {
        auto type = f->getField()->getType();
        switch (type)
        {
        case pvd::Type::scalar:
            {
                processScalar(static_cast<const pvd::PVScalar*>(f), packer);
                break;
            }
        case pvd::Type::scalarArray:
            {
                processScalarArray(static_cast<const pvd::PVScalarArray*>(f), packer);
                break;
            }

        default: break;
        }
    }
}

REGISTER_SERIALIZER(SerializationType::MsgpackCompact, MsgpackCompactSerializer)

SerializedMessageShrdPtr MsgpackCompactSerializer::serialize(const ChannelData& message, const std::string& reply_id)
{
    std::vector<const pvd::PVField*>  values;
    auto                              result = std::make_shared<MsgpackMessage>();
    msgpack::packer<msgpack::sbuffer> packer(result->getBuffer());

    // process root structure
    scannStructure(message.data.get(), values);

    // check if we need to add the reply id
    if (reply_id.empty())
    {
        packer.pack_array(values.size() + 1);
    }
    else
    {
        packer.pack_array(values.size() + 2);
        // pack reply id before the name
        packer.pack(reply_id);
    }
    packer.pack(message.pv_name);
    // serialize
    for (auto& f : values)
    {
        auto type = f->getField()->getType();
        switch (type)
        {
        case pvd::Type::scalar:
            {
                processScalar(static_cast<const pvd::PVScalar*>(f), packer);
                break;
            }
        case pvd::Type::scalarArray:
            {
                processScalarArray(static_cast<const pvd::PVScalarArray*>(f), packer);
                break;
            }

        default: break;
        }
    }
    return result;
}

void MsgpackCompactSerializer::processScalar(const pvd::PVScalar* scalar, msgpack::packer<msgpack::sbuffer>& packer)
{
    switch (scalar->getScalar()->getScalarType())
    {
    case pvd::ScalarType::pvBoolean:
        {
            packer.pack(scalar->getAs<pvd::boolean>());
            break;
        }
    case pvd::ScalarType::pvByte:
        {
            packer.pack(scalar->getAs<pvd::int8>());
            break;
        }
    case pvd::ScalarType::pvDouble:
        {
            packer.pack(scalar->getAs<double>());
            break;
        }
    case pvd::ScalarType::pvFloat:
        {
            packer.pack(scalar->getAs<float>());
            break;
        }
    case pvd::ScalarType::pvInt:
        {
            packer.pack(scalar->getAs<pvd::int32>());
            break;
        }
    case pvd::ScalarType::pvLong:
        {
            packer.pack(scalar->getAs<pvd::int64>());
            break;
        }
    case pvd::ScalarType::pvShort:
        {
            packer.pack(scalar->getAs<pvd::int16>());
            break;
        }
    case pvd::ScalarType::pvString:
        {
            packer.pack(scalar->getAs<std::string>());
            break;
        }
    case pvd::ScalarType::pvUByte:
        {
            packer.pack(scalar->getAs<pvd::uint8>());
            break;
        }
    case pvd::ScalarType::pvUInt:
        {
            packer.pack(scalar->getAs<pvd::uint32>());
            break;
        }
    case pvd::ScalarType::pvULong:
        {
            packer.pack(scalar->getAs<pvd::uint64>());
            break;
        }
    case pvd::ScalarType::pvUShort:
        {
            packer.pack(scalar->getAs<pvd::uint16>());
            break;
        }
    }
}

#define PACK_ARRAY(t, arr, packer)                                 \
  auto converted_array = pvd::shared_vector_convert<const t>(arr); \
  packer.pack_array(converted_array.size());                       \
  for (auto& e : converted_array) { packer.pack(e); }

void MsgpackCompactSerializer::processScalarArray(const pvd::PVScalarArray* scalarArray, msgpack::packer<msgpack::sbuffer>& packer)
{
    pvd::shared_vector<const void> arr;
    scalarArray->getAs<const void>(arr);
    // packer.pack_bin(arr.size());
    // packer.pack_bin_body(static_cast<const char*>(arr.data()), arr.size());
    switch (scalarArray->getScalarArray()->getElementType())
    {
    case pvd::ScalarType::pvBoolean:
        {
            PACK_ARRAY(pvd::boolean, arr, packer)
            break;
        }
    case pvd::ScalarType::pvByte:
        {
            PACK_ARRAY(pvd::int8, arr, packer)
            break;
        }
    case pvd::ScalarType::pvDouble:
        {
            PACK_ARRAY(double, arr, packer)
            break;
        }
    case pvd::ScalarType::pvFloat:
        {
            PACK_ARRAY(float, arr, packer)
            break;
        }
    case pvd::ScalarType::pvInt:
        {
            PACK_ARRAY(pvd::int32, arr, packer)
            break;
        }
    case pvd::ScalarType::pvLong:
        {
            PACK_ARRAY(pvd::int64, arr, packer)
            break;
        }
    case pvd::ScalarType::pvShort:
        {
            PACK_ARRAY(pvd::int8, arr, packer)
            break;
        }
    case pvd::ScalarType::pvString:
        {
            PACK_ARRAY(std::string, arr, packer)
            break;
        }
    case pvd::ScalarType::pvUByte:
        {
            PACK_ARRAY(pvd::uint8, arr, packer)
            break;
        }
    case pvd::ScalarType::pvUInt:
        {
            PACK_ARRAY(pvd::uint32, arr, packer)
            break;
        }
    case pvd::ScalarType::pvULong:
        {
            PACK_ARRAY(pvd::uint64, arr, packer)
            break;
        }
    case pvd::ScalarType::pvUShort:
        {
            PACK_ARRAY(pvd::uint16, arr, packer)
            break;
        }
    }
}

void MsgpackCompactSerializer::scannStructure(const epics::pvData::PVStructure* structure, std::vector<const epics::pvData::PVField*>& values)
{
    const pvd::StructureConstPtr& type = structure->getStructure();
    const pvd::PVFieldPtrArray&   children = structure->getPVFields();
    const pvd::StringArray&       names = type->getFieldNames();

    // init map
    for (size_t i = 0, N = names.size(); i < N; i++)
    {
        auto const& fld = children[i].get();
        // pack key
        auto type = fld->getField()->getType();
        switch (type)
        {
        case pvd::Type::scalar:
            {
                values.push_back(fld);
                break;
            }
        case pvd::Type::scalarArray:
            {
                values.push_back(fld);
                break;
            }
        case pvd::Type::structure:
            {
                scannStructure(static_cast<const pvd::PVStructure*>(fld), values);
                break;
            }
        case pvd::Type::structureArray:
            {
                scannStructureArray(static_cast<const pvd::PVStructureArray*>(fld)->view(), values);
                break;
            }
        default: break;
        }
    }
}

void MsgpackCompactSerializer::scannStructureArray(pvd::PVStructureArray::const_svector structure_array, std::vector<const epics::pvData::PVField*>& values)
{
    for (size_t i = 0, N = structure_array.size(); i < N; i++)
    {
        scannStructure(structure_array[i].get(), values);
    }
}