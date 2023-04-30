#include <k2eg/service/epics/MsgPackSerialization.h>
#include <pv/bitSet.h>

#include <sstream>

#include "pvType.h"

using namespace k2eg::service::epics_impl;

namespace pvd = epics::pvData;

#pragma region MsgPackMessage
MsgPackMessage::MsgPackMessage(epics::pvData::PVStructure::const_shared_pointer epics_pv_struct) : epics_pv_struct(epics_pv_struct) {}
const size_t
MsgPackMessage::size() const {
  return buf.size();
}
const char*
MsgPackMessage::data() const {
  return buf.data();
}
#pragma endregion MsgPackMessage

#pragma region MsgPackSerializer
REGISTER_SERIALIZER(SerializationType::Msgpack, MsgPackSerializer)
SerializedMessageShrdPtr
MsgPackSerializer::serialize(const ChannelData& message) {
  auto                              result = MakeMsgPackMessageShrdPtr(message.data);
  msgpack::packer<msgpack::sbuffer> packer(result->buf);
  // add channel message
  packer.pack_map(1);
  packer.pack(message.pv_name);
  // process root structure
  processStructure(message.data.get(), packer);
  return result;
}

void
MsgPackSerializer::processScalar(const pvd::PVScalar* scalar, msgpack::packer<msgpack::sbuffer>& packer) {
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
MsgPackSerializer::processScalarArray(const pvd::PVScalarArray* scalarArray, msgpack::packer<msgpack::sbuffer>& packer) {
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
MsgPackSerializer::processStructure(const epics::pvData::PVStructure* structure, msgpack::packer<msgpack::sbuffer>& packer) {
  const pvd::StructureConstPtr& type     = structure->getStructure();
  const pvd::PVFieldPtrArray&   children = structure->getPVFields();
  const pvd::StringArray&       names    = type->getFieldNames();

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
      case pvd::Type::structureArray: {
        processStructureArray(static_cast<const pvd::PVStructureArray*>(fld)->view(), packer);
        break;
      }
    }
  }
}

void
MsgPackSerializer::processStructureArray(pvd::PVStructureArray::const_svector structure_array, msgpack::packer<msgpack::sbuffer>& packer) {
  packer.pack_array(structure_array.size());
  for (size_t i = 0, N = structure_array.size(); i < N; i++) { processStructure(structure_array[i].get(), packer); }
}

#pragma endregion MsgPackSerializer