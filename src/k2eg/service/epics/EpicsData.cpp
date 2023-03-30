#include <k2eg/service/epics/EpicsData.h>

using namespace k2eg::service::epics_impl;

ConstSerializedMessageUPtr serialize_epics_data(const ChannelData& message, SerializationType type) {
    ConstSerializedMessageUPtr result;
    switch (type) {
    case SerializationType::JSON:
        break;
    case SerializationType::MessagePack:
        break;
    case SerializationType::Undefined:
        break;
    }
    return result;
}
