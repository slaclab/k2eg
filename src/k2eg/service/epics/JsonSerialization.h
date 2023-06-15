#ifndef K2EG_SERVICE_EPICS_JSONSERIALIZATION_H_
#define K2EG_SERVICE_EPICS_JSONSERIALIZATION_H_

#include <k2eg/service/epics/Serialization.h>
#include <string>
#include <boost/json.hpp>
#include "pvData.h"
namespace k2eg::service::epics_impl {

// JSON serializer
class JsonSerializer : public Serializer {
    void processField(const epics::pvData::PVField* scalar, const std::string& key, boost::json::object& json_object);
    void processScalar(const epics::pvData::PVScalar* scalar, const std::string& key, boost::json::object& json_object);
    void processScalarArray(const epics::pvData::PVScalarArray* scalarArray,const std::string& key,  boost::json::object& json_object);
    void processStructure(const epics::pvData::PVStructure* scalarArray, const std::string& key,  boost::json::object& json_object);
    void processStructureArray(epics::pvData::PVStructureArray::const_svector structure_array, const std::string& key, boost::json::object& json_object);
public:
    JsonSerializer() = default;
    virtual ~JsonSerializer() = default;
    k2eg::common::SerializedMessageShrdPtr serialize(const ChannelData& message, const std::string& reply_id = "");
};
DEFINE_PTR_TYPES(JsonSerializer)
// Serialization message for json encoding
class JsonMessage : public k2eg::common::SerializedMessage {
    friend class JsonSerializer;
    std::string json_object;
    public:
    JsonMessage(std::string& json_object);
    JsonMessage() = delete;
    ~JsonMessage()=default;
    const size_t size() const;
    const char* data() const;
};
DEFINE_PTR_TYPES(JsonMessage)

} // namespace k2eg::service::epics_impl

#endif // K2EG_SERVICE_EPICS_JSONSERIALIZATION_H_