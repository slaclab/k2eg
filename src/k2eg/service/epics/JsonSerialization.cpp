#include <k2eg/service/epics/JsonSerialization.h>
#include <k2eg/controller/command/cmd/Command.h>
#include <pv/bitSet.h>
#include <pv/json.h>

#include <sstream>

#include <boost/json/array.hpp>
#include <pvData.h>
#include <pvIntrospect.h>
using namespace k2eg::common;
using namespace k2eg::service::epics_impl;
namespace pvd = epics::pvData;

#pragma region JsonSerializer
REGISTER_SERIALIZER(SerializationType::JSON, JsonSerializer)
SerializedMessageShrdPtr
JsonSerializer::serialize(const ChannelData& message, const std::string& reply_id) {
  std::stringstream       ss;
  boost::json::object     json_root_object;
  // boost::json::serializer sr;
  processStructure(message.data.get(), message.pv_name, json_root_object);
  if(!reply_id.empty()){json_root_object[KEY_REPLY_ID] = reply_id;}
  ss << json_root_object;
  return MakeJsonMessageShrdPtr(std::move(ss.str()));
}

void
JsonSerializer::processScalar(const pvd::PVScalar* scalar, const std::string& key, boost::json::object& json_object) {
  switch (scalar->getScalar()->getScalarType()) {
    case pvd::ScalarType::pvBoolean: {
      json_object[key] = scalar->getAs<pvd::boolean>();
      break;
    }
    case pvd::ScalarType::pvByte: {
      json_object[key] = scalar->getAs<pvd::int8>();
      break;
    }
    case pvd::ScalarType::pvDouble: {
      const double double_value = scalar->getAs<double>();
      if (std::isnan(double_value)) {
        json_object[key] = "NaN";
      } else {
        json_object[key] = double_value;
      }
      break;
    }
    case pvd::ScalarType::pvFloat: {
      const float float_value = scalar->getAs<float>();
      if (std::isnan(float_value)) {
        json_object[key] = "NaN";
      } else {
        json_object[key] = float_value;
      }
      break;
    }
    case pvd::ScalarType::pvInt: {
      json_object[key] = scalar->getAs<pvd::int32>();
      break;
    }
    case pvd::ScalarType::pvLong: {
      json_object[key] = scalar->getAs<pvd::int64>();
      break;
    }
    case pvd::ScalarType::pvShort: {
      json_object[key] = scalar->getAs<pvd::int16>();
      break;
    }
    case pvd::ScalarType::pvString: {
      json_object[key] = scalar->getAs<std::string>();
      break;
    }
    case pvd::ScalarType::pvUByte: {
      json_object[key] = scalar->getAs<pvd::uint8>();
      break;
    }
    case pvd::ScalarType::pvUInt: {
      json_object[key] = scalar->getAs<pvd::uint32>();
      break;
    }
    case pvd::ScalarType::pvULong: {
      json_object[key] = scalar->getAs<pvd::uint64>();
      break;
    }
    case pvd::ScalarType::pvUShort: {
      json_object[key] = scalar->getAs<pvd::uint16>();
      break;
    }
  }
}
#define TO_JSON_ARRAY(t, arr, json_object)                                       \
  boost::json::array jarr;                                                       \
  auto               converted_array = pvd::shared_vector_convert<const t>(arr); \
  for (auto& e : converted_array) { jarr.emplace_back(e); }                      \
  json_object = jarr;

void
JsonSerializer::processScalarArray(const pvd::PVScalarArray* scalarArray, const std::string& key, boost::json::object& json_object) {
  pvd::shared_vector<const void> arr;
  scalarArray->getAs<const void>(arr);
  switch (scalarArray->getScalarArray()->getElementType()) {
    case pvd::ScalarType::pvBoolean: {
      TO_JSON_ARRAY(pvd::boolean, arr, json_object[key])
      break;
    }
    case pvd::ScalarType::pvByte: {
      TO_JSON_ARRAY(pvd::int8, arr, json_object[key])
      break;
    }
    case pvd::ScalarType::pvDouble: {
      TO_JSON_ARRAY(double, arr, json_object[key])
      break;
    }
    case pvd::ScalarType::pvFloat: {
      TO_JSON_ARRAY(float, arr, json_object[key])
      break;
    }
    case pvd::ScalarType::pvInt: {
      TO_JSON_ARRAY(pvd::int32, arr, json_object[key])
      break;
    }
    case pvd::ScalarType::pvLong: {
      TO_JSON_ARRAY(pvd::int64, arr, json_object[key])
      break;
    }
    case pvd::ScalarType::pvShort: {
      TO_JSON_ARRAY(pvd::int8, arr, json_object[key])
      break;
    }
    case pvd::ScalarType::pvString: {
      TO_JSON_ARRAY(std::string, arr, json_object[key])
      break;
    }
    case pvd::ScalarType::pvUByte: {
      TO_JSON_ARRAY(pvd::uint8, arr, json_object[key])
      break;
    }
    case pvd::ScalarType::pvUInt: {
      TO_JSON_ARRAY(pvd::uint32, arr, json_object[key])
      break;
    }
    case pvd::ScalarType::pvULong: {
      TO_JSON_ARRAY(pvd::uint64, arr, json_object[key])
      break;
    }
    case pvd::ScalarType::pvUShort: {
      TO_JSON_ARRAY(pvd::uint16, arr, json_object[key])
      break;
    }
  }
}

void
JsonSerializer::processField(const epics::pvData::PVField* field, const std::string& key, boost::json::object& json_object) {
  switch (field->getField()->getType()) {
    case pvd::scalar: {
      const pvd::PVScalar* scalar = static_cast<const pvd::PVScalar*>(field);
      processScalar(scalar, key, json_object);
      break;
    }
    case pvd::scalarArray: {
      const pvd::PVScalarArray* scalarArray = static_cast<const pvd::PVScalarArray*>(field);
      processScalarArray(scalarArray, key, json_object);
      break;
    }
    case pvd::structure: processStructure(static_cast<const pvd::PVStructure*>(field), key, json_object); break;
    case pvd::structureArray: {
      processStructureArray(static_cast<const pvd::PVStructureArray*>(field)->view(), key, json_object);
      break;
    }
    case pvd::union_: {
      const pvd::PVUnion*                       U = static_cast<const pvd::PVUnion*>(field);
      const pvd::PVField::const_shared_pointer& sub_field(U->get());
      processField(sub_field.get(), key, json_object);
      break;
    }
      return;
    case pvd::unionArray: {
      const pvd::PVUnionArray*         U = static_cast<const pvd::PVUnionArray*>(field);
      pvd::PVUnionArray::const_svector arr(U->view());
      break;
    }
  }
}

void
JsonSerializer::processStructure(const epics::pvData::PVStructure* structure, const std::string& key, boost::json::object& json_object) {
  const pvd::StructureConstPtr& type     = structure->getStructure();
  const pvd::PVFieldPtrArray&   children = structure->getPVFields();
  const pvd::StringArray&       names    = type->getFieldNames();
  boost::json::object           struct_obj;
  for (size_t i = 0, N = names.size(); i < N; i++) {
    auto const& fld  = children[i].get();
    auto        type = fld->getField()->getType();
    switch (type) {
      case pvd::Type::scalar: {
        processScalar(static_cast<const pvd::PVScalar*>(fld), names[i], struct_obj);
        break;
      }
      case pvd::Type::scalarArray: {
        processScalarArray(static_cast<const pvd::PVScalarArray*>(fld), names[i], struct_obj);
        break;
      }
      case pvd::Type::structure: {
        processStructure(static_cast<const pvd::PVStructure*>(fld), names[i], struct_obj);
        break;
      }
      case pvd::Type::structureArray: {
        processStructureArray(static_cast<const pvd::PVStructureArray*>(fld)->view(), names[i], struct_obj);
        break;
      }

      case pvd::Type::union_: {
        const pvd::PVUnion*                       union_field = static_cast<const pvd::PVUnion*>(fld);
        const pvd::PVField::const_shared_pointer& field(union_field->get());
        if (field) { processField(field.get(), key, json_object); }
        break;
      }

      case pvd::Type::unionArray: {
        const pvd::PVUnionArray*         U = static_cast<const pvd::PVUnionArray*>(fld);
        pvd::PVUnionArray::const_svector arr(U->view());

        // for(size_t i=0, N=arr.size(); i<N; i++) {
        //     if(arr[i])
        //         show_field(A, arr[i].get(), 0);
        //     else
        //         yg(yajl_gen_null(A.handle));
        // }

        // yg(yajl_gen_array_close(A.handle));
        break;
      }
    }
  }
  if (!key.empty()) { json_object[key] = struct_obj; }
}

void
JsonSerializer::processStructureArray(pvd::PVStructureArray::const_svector structure_array, const std::string& key, boost::json::object& json_object) {
  boost::json::array array_obj;
  for (size_t i = 0, N = structure_array.size(); i < N; i++) {
    boost::json::object arr_element_object;
    processStructure(structure_array[i].get(), "", arr_element_object);
    array_obj.push_back(arr_element_object);
  }
  json_object[key] = array_obj;
}

#pragma endregion JsonSerializer