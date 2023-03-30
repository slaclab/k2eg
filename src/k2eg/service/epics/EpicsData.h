#ifndef K2EG_SERVICE_EPICS_EPICSDATA_H_
#define K2EG_SERVICE_EPICS_EPICSDATA_H_

#include <k2eg/common/ObjectFactory.h>
#include <k2eg/common/types.h>
#include <pv/pvData.h>

#include <memory>
#include <set>
namespace k2eg::service::epics_impl {

// Epics Channel Data
typedef struct {
    const std::string channel_name;
    epics::pvData::PVStructure::const_shared_pointer data;
} ChannelData;
DEFINE_PTR_TYPES(ChannelData)

// define the type of the supported serailization
enum class SerializationType { Undefined, JSON, MessagePack };

// the serialized message
class SerializedMessage {
    public:
    SerializedMessage()=default;
    ~SerializedMessage()=default;
    virtual const size_t size() const = 0;
    virtual const char * data()const = 0;
};
DEFINE_PTR_TYPES(SerializedMessage);

// base serializer class
class Serializer {
public:
    virtual ConstSerializedMessageUPtr serialize(const ChannelData& message) = 0;
};
DEFINE_PTR_TYPES(Serializer)

// define the single instance of epics serializer factory
inline k2eg::common::ObjectByTypeFactory<SerializationType, Serializer> epics_serializer_factory;

// serilizer entry point
inline ConstSerializedMessageUPtr serialize(const ChannelData& message, SerializationType type) {
    // check if serilizer is present
    if (!epics_serializer_factory.hasType(type)) {
        return ConstSerializedMessageUPtr();
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

} // namespace k2eg::service::epics_impl

#endif // K2EG_SERVICE_EPICS_EPICSDATA_H_