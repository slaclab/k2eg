
#ifndef TYPES_H
#define TYPES_H
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <msgpack.hpp>
#include <boost/json.hpp>

namespace k2eg::common
{
#define DEFINE_PTR_TYPES(x) \
typedef std::unique_ptr<x> x##UPtr; \
typedef std::unique_ptr<const x> Const##x##UPtr; \
typedef std::shared_ptr<x> x##ShrdPtr; \
typedef std::shared_ptr<const x> Const##x##ShrdPtr; \
template<typename... _Args> \
inline x##UPtr Make##x##UPtr(_Args&... __args) \
{ \
    return std::make_unique<x>(__args...); \
} \
template<typename... _Args> \
inline x##UPtr Make##x##UPtr(_Args&&... __args) \
{ \
    return std::make_unique<x>(__args...); \
} \
template<typename... _Args> \
inline x##ShrdPtr Make##x##ShrdPtr(_Args&... __args) \
{ \
    return std::make_shared<x>(__args...); \
} \
template<typename... _Args> \
inline x##ShrdPtr Make##x##ShrdPtr(_Args&&... __args) \
{ \
    return std::make_shared<x>(__args...); \
}

#define DEFINE_VECTOR_FOR_TYPE(t, n)              \
    typedef std::vector<t> n;                     \
    typedef std::vector<t>::iterator n##Iterator; \
    typedef std::vector<t>::const_iterator n##ConstIterator;

#define DEFINE_MAP_FOR_TYPE(t1, t2, n)                         \
    typedef std::map<t1, t2> n;                                \
    typedef std::map<t1, t2>::iterator n##Iterator;            \
    typedef std::map<t1, t2>::const_iterator n##ConstIterator; \
    typedef std::pair<t1, t2> n##Pair;

    DEFINE_VECTOR_FOR_TYPE(std::string, StringVector);

    DEFINE_MAP_FOR_TYPE(std::string, std::string, MapStrKV);

enum class SerializationType: std::uint8_t { Unknown, JSON, Msgpack, MsgpackCompact };
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


class SerializedMessage {
    public:
    SerializedMessage()=default;
    virtual ~SerializedMessage()=default;
    virtual const size_t size() const = 0;
    virtual const char * data()const = 0;
};
DEFINE_PTR_TYPES(SerializedMessage);

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

#endif // __TYPES_H__