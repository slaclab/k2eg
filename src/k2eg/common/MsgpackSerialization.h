#ifndef K2EG_COMMON_MSGPACKSERIALIZATION_H_
#define K2EG_COMMON_MSGPACKSERIALIZATION_H_
#include <k2eg/common/BaseSerialization.h>
#include <msgpack.hpp>

namespace k2eg::common {
/**
*/
class MsgpackMessage : public k2eg::common::SerializedMessage {
    public:
    msgpack::sbuffer buf;
    MsgpackMessage() = default;
    ~MsgpackMessage()=default;
    const size_t
    size() const {
    return buf.size();
    }
    const char*
    data() const {
    return buf.data();
    }
};
DEFINE_PTR_TYPES(MsgpackMessage)
}

#endif // K2EG_COMMON_MSGPACKSERIALIZATION_H_namespace name = ;