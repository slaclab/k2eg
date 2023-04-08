#ifndef K2EG_SERVICE_EPICS_JSONSERIALIZATION_H_
#define K2EG_SERVICE_EPICS_JSONSERIALIZATION_H_

#include <k2eg/service/epics/Serialization.h>
#include <string>
#include <boost/json.hpp>
namespace k2eg::service::epics_impl {

// JSON serializer
class JsonSerializer : public Serializer {
    void processScalar(const epics::pvData::PVScalar* scalar, const std::string& key, boost::json::object& json_object);
    void processScalarArray(const epics::pvData::PVScalarArray* scalarArray,const std::string& key,  boost::json::object& json_object);
    void processStructure(const epics::pvData::PVStructure* scalarArray,const std::string& key,  boost::json::object& json_object);
public:
    JsonSerializer() = default;
    virtual ~JsonSerializer() = default;
    ConstSerializedMessageUPtr serialize(const ChannelData& message);
};
DEFINE_PTR_TYPES(JsonSerializer)
// Serialization message for json encoding
class JsonMessage : public SerializedMessage {
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