#ifndef K2EG_COMMON_BASESERIALIZATION_H_
#define K2EG_COMMON_BASESERIALIZATION_H_

#include <k2eg/common/types.h>

#include <algorithm>
#include <cctype>

namespace k2eg::common {
enum class SerializationType : std::uint8_t { Unknown, Msgpack, MsgpackCompact, JSON};
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



inline constexpr SerializationType
serialization_from_string(const std::string& t) noexcept {
  auto to_lower = [](const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return lower;
  };
  std::string lt = to_lower(t);
  if (lt == "json") return SerializationType::JSON;
  if (lt == "msgpack") return SerializationType::Msgpack;
  if (lt == "msgpack-compact") return SerializationType::MsgpackCompact;
  if (lt == "unknown") return SerializationType::Unknown;
  return SerializationType::Unknown;
}

static void
tag_invoke(boost::json::value_from_tag, boost::json::value &jv, k2eg::common::SerializationType const &ser) {
  jv = {{"type", serialization_to_string(ser)}};
}

class Data {
 public:
  Data()                            = default;
  virtual ~Data()                   = default;
  virtual const size_t size() const = 0;
  virtual const char  *data() const = 0;
};
DEFINE_PTR_TYPES(Data)
/**
 */
class SerializedMessage {
 public:
  SerializedMessage()                = default;
  virtual ~SerializedMessage()       = default;
  virtual ConstDataUPtr data() const = 0;
};
DEFINE_PTR_TYPES(SerializedMessage);
}  // namespace k2eg::common

#endif  // K2EG_COMMON_BASESERIALIZATION_H_
