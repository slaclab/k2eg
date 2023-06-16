#ifndef K2EG_CONTROLLER_COMMAND_CMD_COMMAND_H_
#define K2EG_CONTROLLER_COMMAND_CMD_COMMAND_H_

#include <k2eg/common/types.h>

#include <boost/json.hpp>
#include <cstdint>
#include <string>

namespace k2eg::controller::command::cmd {

// are all the possible command
enum class CommandType { monitor, get, put, info, unknown };
constexpr const char *
command_type_to_string(CommandType t) noexcept {
  switch (t) {
    case CommandType::get: return "get";
    case CommandType::info: return "info";
    case CommandType::monitor: return "monitor";
    case CommandType::put: return "put";
    case CommandType::unknown: return "unknown";
  }
  return "undefined";
}

#define KEY_COMMAND       "command"
#define KEY_SERIALIZATION "serialization"
#define KEY_PROTOCOL      "protocol"
#define KEY_PV_NAME       "pv_name"
#define KEY_ACTIVATE      "activate"
#define KEY_DEST_TOPIC    "dest_topic"
#define KEY_VALUE         "value"
#define KEY_REPLY_ID      "reply_id"

/**
Base command structure
*/
struct Command {
  CommandType       type;
  k2eg::common::SerializationType serialization;
  std::string       protocol;
  std::string       pv_name;
};
DEFINE_PTR_TYPES(Command)

typedef std::vector<ConstCommandShrdPtr> ConstCommandShrdPtrVec;

static void
tag_invoke(boost::json::value_from_tag, boost::json::value &jv, CommandType const &cfg) {
  jv = {{"type", command_type_to_string(cfg)}};
}


static void
tag_invoke(boost::json::value_from_tag, boost::json::value &jv, Command const &c) {
  jv = {{"serialization", serialization_to_string(c.serialization)}, {"pv_name", c.pv_name}, {"protocol", c.protocol}

  };
}

/**
Base command reply structure

Some command cand send a reply as result of operation,
thi class represent the base information for a reply
*/
struct CommandReply {
  //[mandatory] si the error code of the operation done by the command
  const std::int8_t error_code;
  //[mandatory] is the request id found on the command that has generated the reply
  const std::string reply_id;
};

static void
tag_invoke(boost::json::value_from_tag, boost::json::value &jv, CommandReply const &reply) {
  jv = {{"error_code", reply.error_code}, {KEY_REPLY_ID, reply.reply_id}};
}


}  // namespace k2eg::controller::command::cmd

#endif  // K2EG_CONTROLLER_COMMAND_CMD_COMMAND_H_