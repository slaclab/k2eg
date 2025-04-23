#ifndef K2EG_SERVICE_EPICS_SERIALIZATION_H_
#define K2EG_SERVICE_EPICS_SERIALIZATION_H_
#include "k2eg/common/BaseSerialization.h"
#include "k2eg/common/JsonSerialization.h"
#include <k2eg/common/serialization.h>
#include <k2eg/common/types.h>
#include <k2eg/service/epics/EpicsData.h>

namespace k2eg::service::epics_impl {
// define the type of the supported serailization

// base serializer class
class Serializer
{
public:
    virtual k2eg::common::SerializedMessageShrdPtr serialize(const ChannelData& message, const std::string& reply_id = "") = 0;
    virtual void serialize(const ChannelData& message, common::SerializedMessage& serialized_message) = 0;
};
DEFINE_PTR_TYPES(Serializer)

// define the single instance of epics serializer factory
inline k2eg::common::ObjectByTypeFactory<k2eg::common::SerializationType, Serializer> epics_serializer_factory;

// serilizer entry point
inline k2eg::common::ConstSerializedMessageShrdPtr serialize(const ChannelData& message, k2eg::common::SerializationType type, const std::string& reply_id = "")
{
    // check if serilizer is present
    if (!epics_serializer_factory.hasType(type))
    {
        return k2eg::common::ConstSerializedMessageShrdPtr();
    }
    return epics_serializer_factory.resolve(type)->serialize(message, reply_id);
}

// check for a serializer for a specific type
inline bool has_serialization_for_type(k2eg::common::SerializationType type)
{
    return epics_serializer_factory.hasType(type);
}

// helper for serializer registration
template <typename T, typename S>
class SerializerRegister
{
public:
    SerializerRegister(T type)
    {
        epics_serializer_factory.registerObjectInstance(type, std::make_shared<S>());
    }
};

#define REGISTER_SERIALIZER(T, S) \
SerializerRegister<SerializationType, S> S##_serializer_register(T);
} // namespace k2eg::service::epics_impl

#endif // K2EG_SERVICE_EPICS_SERIALIZATION_H_