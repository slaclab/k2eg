#ifndef K2EG_SERVICE_EPICS_SERIALIZATION_H_
#define K2EG_SERVICE_EPICS_SERIALIZATION_H_
#include <k2eg/service/epics/EpicsData.h>

namespace k2eg::service::epics_impl {
// define the type of the supported serailization
enum class SerializationType: std::uint8_t { Unknown, JSON, Msgpack, MsgpackCompact };

// base serialized message
class SerializedMessage {
    public:
    SerializedMessage()=default;
    virtual ~SerializedMessage()=default;
    virtual const size_t size() const = 0;
    virtual const char * data()const = 0;
};
DEFINE_PTR_TYPES(SerializedMessage);

// base serializer class
class Serializer {
public:
    virtual SerializedMessageShrdPtr serialize(const ChannelData& message) = 0;
};
DEFINE_PTR_TYPES(Serializer)

// define the single instance of epics serializer factory
inline k2eg::common::ObjectByTypeFactory<SerializationType, Serializer> epics_serializer_factory;

// serilizer entry point
inline ConstSerializedMessageShrdPtr serialize(const ChannelData& message, SerializationType type) {
    // check if serilizer is present
    if (!epics_serializer_factory.hasType(type)) {
        return ConstSerializedMessageShrdPtr();
    }
    return epics_serializer_factory.resolve(type)->serialize(message) ;
}

// check for a serializer for a specific type
inline bool has_serialization_for_type(SerializationType type) { 
    return epics_serializer_factory.hasType(type);
}

// helper for serializer registration
template <typename T, typename S> class SerializerRegister {
public:
    SerializerRegister(T type) { epics_serializer_factory.registerObjectInstance(type, std::make_shared<S>()); }
};

#define REGISTER_SERIALIZER(T, S) \
SerializerRegister<SerializationType, S> S##_serializer_register(T);
}

#endif // K2EG_SERVICE_EPICS_SERIALIZATION_H_