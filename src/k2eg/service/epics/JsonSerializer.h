#ifndef K2EG_SERVICE_EPICS_JSONSERIALIZATION_H_
#define K2EG_SERVICE_EPICS_JSONSERIALIZATION_H_

#include <k2eg/service/epics/EpicsData.h>

namespace k2eg::service::epics_impl {

class JsonSerializer : public Serializer {
public:
    JsonSerializer() = default;
    virtual ~JsonSerializer() = default;
    ConstSerializedMessageUPtr serialize(const ChannelData& message);
};

} // namespace k2eg::service::epics_impl

#endif // K2EG_SERVICE_EPICS_JSONSERIALIZATION_H_