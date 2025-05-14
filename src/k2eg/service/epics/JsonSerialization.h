#ifndef K2EG_SERVICE_EPICS_JSONSERIALIZATION_H_
#define K2EG_SERVICE_EPICS_JSONSERIALIZATION_H_

#include <k2eg/service/epics/Serialization.h>

#include <boost/json.hpp>

#include <pvData.h>

#include <string>
namespace k2eg::service::epics_impl {

// JSON serializer
class JsonSerializer : public Serializer
{
    void processField(const epics::pvData::PVField* scalar, const std::string& key, boost::json::object& json_object);
    void processScalar(const epics::pvData::PVScalar* scalar, const std::string& key, boost::json::object& json_object);
    void processScalarArray(const epics::pvData::PVScalarArray* scalarArray, const std::string& key, boost::json::object& json_object);
    void processStructure(const epics::pvData::PVStructure* scalarArray, const std::string& key, boost::json::object& json_object);
    void processStructureArray(epics::pvData::PVStructureArray::const_svector structure_array, const std::string& key, boost::json::object& json_object);

public:
    JsonSerializer() = default;
    virtual ~JsonSerializer() = default;
    void serialize(const ChannelData& message, common::SerializedMessage& serialized_message);
    k2eg::common::SerializedMessageShrdPtr serialize(const ChannelData& message, const std::string& reply_id = "");
};
DEFINE_PTR_TYPES(JsonSerializer)
} // namespace k2eg::service::epics_impl

#endif // K2EG_SERVICE_EPICS_JSONSERIALIZATION_H_