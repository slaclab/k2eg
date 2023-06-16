#ifndef K2EG_COMMON_JSONSERIALIZATION_H_
#define K2EG_COMMON_JSONSERIALIZATION_H_

#include <k2eg/common/serialization.h>

namespace k2eg::common {
    /**
*/
class JsonMessage : public k2eg::common::SerializedMessage {
    std::string json_object;
    public:
    JsonMessage(std::string& json_object): json_object(std::move(json_object)) {}
    JsonMessage() = delete;
    ~JsonMessage()=default;
    const size_t
    size() const {
    return json_object.size();
    }
    const char*
    data() const {
    return json_object.c_str();
    }
};
DEFINE_PTR_TYPES(JsonMessage)
}

#endif // K2EG_COMMON_JSONSERIALIZATION_H_