#ifndef K2EG_SERVICE_EPICS_JSONSERIALIZATION_H_
#define K2EG_SERVICE_EPICS_JSONSERIALIZATION_H_

#include <k2eg/service/epics/EpicsData.h>
#include <string>
namespace k2eg::service::epics_impl {

// JSON serializer
class JsonSerializer : public Serializer {
public:
    JsonSerializer() = default;
    virtual ~JsonSerializer() = default;
    ConstSerializedMessageUPtr serialize(const ChannelData& message);
};

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