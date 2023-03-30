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
struct SerializedMessage {
    const size_t data_len;
    std::unique_ptr<const char[]> data;
};
DEFINE_PTR_TYPES(SerializedMessage);

// base serializer class
class Serializer {
public:
    virtual ConstSerializedMessageUPtr serialize(const ChannelData& message) = 0;
};
DEFINE_PTR_TYPES(Serializer)

//DEFINE_MAP_FOR_TYPE(SerializationType, SerializerShrdPtr, SerializerMap);
static k2eg::common::ObjectByTypeFactory<SerializationType, Serializer> serializer_factory;

inline ConstSerializedMessageUPtr serialize_epics_data(const ChannelData& message, SerializationType type) {
    // check if serilizer is present
    if (!serializer_factory.hasType(type)) {
        return ConstSerializedMessageUPtr();
    }
    return serializer_factory.resolve(type)->serialize(message) ;
}

inline bool has_serialization_for_type(SerializationType type) { 
    return serializer_factory.hasType(type);
}

template <typename T, typename S> class SerializerRegister {
public:
    SerializerRegister(T type) { serializer_factory.registerObjectInstance(type, std::make_shared<S>()); }
};

#define REGISTER_SERIALIZER(T, S) \
static SerializerRegister<SerializationType, S> S##_serializer_register(T);

} // namespace k2eg::service::epics_impl

#endif // K2EG_SERVICE_EPICS_EPICSDATA_H_