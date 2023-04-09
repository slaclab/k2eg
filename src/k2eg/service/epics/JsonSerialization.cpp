#include <k2eg/service/epics/JsonSerialization.h>

#include <pv/bitSet.h>
#include <pv/json.h>

#include <sstream>
using namespace k2eg::service::epics_impl;
namespace pvd = epics::pvData;

#pragma region JsonMessage
JsonMessage::JsonMessage(std::string& json_object)
    : json_object(std::move(json_object)) {}

const size_t JsonMessage::size() const {
    return json_object.size();
}
const char* JsonMessage::data() const {
    return json_object.c_str();
}
#pragma endregion JsonMessage


#pragma region JsonSerializer
REGISTER_SERIALIZER(SerializationType::JSON, JsonSerializer)
ConstSerializedMessageUPtr JsonSerializer::serialize(const ChannelData& message) {
    std::stringstream ss;
    boost::json::object json_root_object;
    processStructure(message.data.get(), message.channel_name, json_root_object);
    // ss << "{\"" << message.channel_name << "\":{";
    // epics::pvData::BitSet bs_mask;
    // epics::pvData::JSONPrintOptions opt;
    // opt.ignoreUnprintable = true;
    // opt.indent = 0;
    // opt.multiLine = false;
    // for (size_t idx = 1, N = message.data->getStructure()->getNumberFields(); idx < N; idx++) {
    //     bs_mask.set(idx);
    // }
    // epics::pvData::printJSON(ss, *message.data, bs_mask, opt);
    // ss << "}";
    ss << json_root_object;
    return MakeJsonMessageUPtr(std::move(ss.str()));
}

void JsonSerializer::processScalar(const pvd::PVScalar* scalar, const std::string& key, boost::json::object& json_object) {
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
        if(std::isnan(double_value)) {
            json_object[key] = "NaN";
        } else {
            json_object[key] = double_value;
        }
        break;
    }
    case pvd::ScalarType::pvFloat: {
        const float float_value = scalar->getAs<float>();
        if(std::isnan(float_value)) {
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

void JsonSerializer::processScalarArray(const pvd::PVScalarArray* scalarArray, const std::string& key,  boost::json::object& json_object) {
}

void JsonSerializer::processStructure(const epics::pvData::PVStructure* structure, const std::string& key, boost::json::object& json_object) {
    const pvd::StructureConstPtr& type = structure->getStructure();
    const pvd::PVFieldPtrArray& children = structure->getPVFields();
    const pvd::StringArray& names = type->getFieldNames();
    boost::json::object struct_obj; 
    for (size_t i = 0, N = names.size(); i < N; i++) {
        auto const& fld = children[i].get();
        auto type = fld->getField()->getType();
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
        }
    }
    json_object[key] = struct_obj;
}
#pragma endregion JsonSerializer