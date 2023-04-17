#include <k2eg/service/epics/MsgpackCompactSerializion.h>
#include "pvType.h"
#include <vector>
#include <any>

using namespace k2eg::service::epics_impl;

namespace pvd = epics::pvData;

#pragma region MsgpackCompactMessage
MsgpackCompactMessage::MsgpackCompactMessage(epics::pvData::PVStructure::const_shared_pointer epics_pv_struct) : epics_pv_struct(epics_pv_struct) {}
const size_t
MsgpackCompactMessage::size() const {
  return buf.size();
}
const char*
MsgpackCompactMessage::data() const {
  return buf.data();
}
#pragma endregion MsgpackCompactMessage

#pragma region MsgPackSerializer
REGISTER_SERIALIZER(SerializationType::MsgpackCompact, MsgpackCompactSerializer)
SerializedMessageShrdPtr
MsgpackCompactSerializer::serialize(const ChannelData& message) {
  std::vector<const pvd::PVField*> values;
  auto                              result = MakeMsgpackCompactMessageShrdPtr(message.data);
  msgpack::packer<msgpack::sbuffer> packer(result->buf);
 
  // process root structure
  scannStructure(message.data.get(), values);

  packer.pack_array(values.size()+1);
  packer.pack(message.channel_name);
  //serialize
  for(auto & f: values) {
    auto type = f->getField()->getType();
    switch (type) {
      case pvd::Type::scalar: {
        processScalar(static_cast<const pvd::PVScalar*>(f), packer);
        break;
      }
      case pvd::Type::scalarArray: {
        processScalarArray(static_cast<const pvd::PVScalarArray*>(f), packer);
        break;
      }

      default: break;
    }
  }
  return result;
}

void
MsgpackCompactSerializer::processScalar(const pvd::PVScalar* scalar, msgpack::packer<msgpack::sbuffer>& packer) {
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

#define PACK_ARRAY(t, arr, packer)                                 \
  auto converted_array = pvd::shared_vector_convert<const t>(arr); \
  packer.pack_array(converted_array.size());                       \
  for (auto& e : converted_array) { packer.pack(e); }

void
MsgpackCompactSerializer::processScalarArray(const pvd::PVScalarArray* scalarArray, msgpack::packer<msgpack::sbuffer>& packer) {
  pvd::shared_vector<const void> arr;
  scalarArray->getAs<const void>(arr);
  // packer.pack_bin(arr.size());
  // packer.pack_bin_body(static_cast<const char*>(arr.data()), arr.size());
  switch (scalarArray->getScalarArray()->getElementType()) {
    case pvd::ScalarType::pvBoolean: {
      PACK_ARRAY(pvd::boolean, arr, packer)
      break;
    }
    case pvd::ScalarType::pvByte: {
      PACK_ARRAY(pvd::int8, arr, packer)
      break;
    }
    case pvd::ScalarType::pvDouble: {
      PACK_ARRAY(double, arr, packer)
      break;
    }
    case pvd::ScalarType::pvFloat: {
      PACK_ARRAY(float, arr, packer)
      break;
    }
    case pvd::ScalarType::pvInt: {
      PACK_ARRAY(pvd::int32, arr, packer)
      break;
    }
    case pvd::ScalarType::pvLong: {
      PACK_ARRAY(pvd::int64, arr, packer)
      break;
    }
    case pvd::ScalarType::pvShort: {
      PACK_ARRAY(pvd::int8, arr, packer)
      break;
    }
    case pvd::ScalarType::pvString: {
      PACK_ARRAY(std::string, arr, packer)
      break;
    }
    case pvd::ScalarType::pvUByte: {
      PACK_ARRAY(pvd::uint8, arr, packer)
      break;
    }
    case pvd::ScalarType::pvUInt: {
      PACK_ARRAY(pvd::uint32, arr, packer)
      break;
    }
    case pvd::ScalarType::pvULong: {
      PACK_ARRAY(pvd::uint64, arr, packer)
      break;
    }
    case pvd::ScalarType::pvUShort: {
      PACK_ARRAY(pvd::uint16, arr, packer)
      break;
    }
  }
}

void
MsgpackCompactSerializer::scannStructure(const epics::pvData::PVStructure* structure, std::vector<const epics::pvData::PVField*>& values) {
  const pvd::StructureConstPtr& type     = structure->getStructure();
  const pvd::PVFieldPtrArray&   children = structure->getPVFields();
  const pvd::StringArray&       names    = type->getFieldNames();

  // init map
  for (size_t i = 0, N = names.size(); i < N; i++) {
    auto const& fld = children[i].get();
    // pack key
    auto type = fld->getField()->getType();
    switch (type) {
      case pvd::Type::scalar: {
        values.push_back(fld);
        break;
      }
      case pvd::Type::scalarArray: {
        values.push_back(fld);
        break;
      }
      case pvd::Type::structure: {
        scannStructure(static_cast<const pvd::PVStructure*>(fld), values);
        break;
      }
      case pvd::Type::structureArray: {
        scannStructureArray(static_cast<const pvd::PVStructureArray*>(fld)->view(), values);
        break;
      }
      default: break;
    }
  }
}

void
MsgpackCompactSerializer::scannStructureArray(pvd::PVStructureArray::const_svector structure_array, std::vector<const epics::pvData::PVField*>& values) {
  for (size_t i = 0, N = structure_array.size(); i < N; i++) { scannStructure(structure_array[i].get(), values); }
}

#pragma endregion MsgPackSerializer