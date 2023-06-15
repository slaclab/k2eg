#include <k2eg/common/types.h>
#include <k2eg/controller/node/worker/CommandWorker.h>

#include <boost/json.hpp>
#include <memory>
#include <utility>
#include <vector>
using namespace k2eg::common;
using namespace k2eg::controller::node::worker;

ReplySerializer::ReplySerializer(std::shared_ptr<boost::json::value> reply_content) : reply_content(reply_content) {}

void
ReplySerializer::makeMsgpack(const boost::json::value& jv, msgpack::packer<msgpack::sbuffer>& packer) {
  if (jv.is_object()) {
    packer.pack_map(jv.get_object().size());
    for (auto const& kv : jv.get_object()) {
      packer.pack(kv.key());
      makeMsgpack(kv.value(), packer);
    }
  } else if (jv.is_array()) {
    packer.pack_array(jv.get_array().size());
    for (auto const& element : jv.get_array()) { makeMsgpack(element, packer); }
  } else if (jv.is_int64()) {
    packer.pack(jv.as_int64());
  } else if (jv.is_uint64()) {
    packer.pack(jv.as_uint64());
  } else if (jv.is_double()) {
    packer.pack(jv.as_double());
  } else if (jv.is_bool()) {
    packer.pack(jv.as_bool());
  } else if (jv.is_string()) {
    packer.pack(std::string_view(jv.as_string()));
  }
}

void
ReplySerializer::makeMsgpackCompact(const boost::json::value& jv, msgpack::packer<msgpack::sbuffer>& packer) {
  if (jv.is_object()) {
    packer.pack_array(jv.get_object().size());
    for (auto const& kv : jv.get_object()) { makeMsgpackCompact(kv.value(), packer); }
  } else if (jv.is_array()) {
    packer.pack_array(jv.get_array().size());
    for (auto const& element : jv.get_array()) { makeMsgpack(element, packer); }
  } else if (jv.is_int64()) {
    packer.pack(jv.as_int64());
  } else if (jv.is_uint64()) {
    packer.pack(jv.as_uint64());
  } else if (jv.is_double()) {
    packer.pack(jv.as_double());
  } else if (jv.is_bool()) {
    packer.pack(jv.as_bool());
  } else if (jv.is_string()) {
    packer.pack(std::string_view(jv.as_string()));
  }
}

ConstSerializedMessageUPtr
ReplySerializer::toJson() {
  std::stringstream   ss;
  boost::json::object json_root_object;
  ss << *reply_content;
  return MakeJsonMessageUPtr(std::move(ss.str()));
}

ConstSerializedMessageUPtr
ReplySerializer::toMsgpack() {
  auto                              result = std::make_unique<MsgpackMessage>();
  msgpack::packer<msgpack::sbuffer> packer(result->buf);
  makeMsgpack(*reply_content, packer);
  return result;
}

ConstSerializedMessageUPtr
ReplySerializer::toMsgpackCompact() {
  auto                              result = std::make_unique<MsgpackMessage>();
  msgpack::packer<msgpack::sbuffer> packer(result->buf);
  makeMsgpackCompact(*reply_content, packer);
  return result;
}

ConstSerializedMessageUPtr
ReplySerializer::serialize(k2eg::common::SerializationType ser_type) {
  switch (ser_type) {
    case SerializationType::JSON: {
      return toJson();
    }
    case SerializationType::Msgpack: {
      return toMsgpack();
    }
    case SerializationType::MsgpackCompact: {
      return toMsgpackCompact();
    }
    default: return ConstSerializedMessageUPtr();
  }
}