#ifndef K2EG_COMMON_BASESERIALIZATION_H_
#define K2EG_COMMON_BASESERIALIZATION_H_

#include <k2eg/common/types.h>

namespace k2eg::common {
enum class SerializationType : std::uint8_t { Unknown, JSON, Msgpack, MsgpackCompact };
inline constexpr const char *
serialization_to_string(SerializationType t) noexcept {
  switch (t) {
    case SerializationType::JSON: return "Json";
    case SerializationType::Msgpack: return "Msgpack";
    case SerializationType::MsgpackCompact: return "Msgpack-Compact";
    case SerializationType::Unknown: return "unknown";
  }
  return "undefined";
}

static void
tag_invoke(boost::json::value_from_tag, boost::json::value &jv, k2eg::common::SerializationType const &ser) {
  jv = {{"type", serialization_to_string(ser)}};
}

/**
 */
class SerializedMessage {
 public:
  SerializedMessage()               = default;
  virtual ~SerializedMessage()      = default;
  virtual const size_t size() const = 0;
  virtual const char  *data() const = 0;
};
DEFINE_PTR_TYPES(SerializedMessage);
}  // namespace k2eg::common

#endif  // K2EG_COMMON_BASESERIALIZATION_H_
