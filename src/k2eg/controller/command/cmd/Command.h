#ifndef K2EG_CONTROLLER_COMMAND_CMD_COMMAND_H_
#define K2EG_CONTROLLER_COMMAND_CMD_COMMAND_H_

#include <k2eg/common/serialization.h>
#include <k2eg/common/types.h>
#include <k2eg/controller/command/cmd/Command.h>

#include <boost/json.hpp>
#include <cstdint>
#include <string>

namespace k2eg::controller::command::cmd {

// are all the possible command
enum class CommandType { monitor, multi_monitor, get, put, info, snapshot,unknown };
constexpr const char *
command_type_to_string(CommandType t) noexcept {
  switch (t) {
    case CommandType::get: return "get";
    case CommandType::info: return "info";
    case CommandType::monitor: return "monitor";
    case CommandType::multi_monitor: return "multi-monitor";
    case CommandType::put: return "put";
    case CommandType::snapshot: return "snapshot";
    case CommandType::unknown: return "unknown";
  }
  return "undefined";
}

#define KEY_COMMAND            "command"
#define KEY_SERIALIZATION      "serialization"
#define KEY_PV_NAME            "pv_name"
#define KEY_PV_NAME_LIST       "pv_name_list"
#define KEY_REPLY_TOPIC        "reply_topic"
#define KEY_MONITOR_DEST_TOPIC "monitor_dest_topic"
#define KEY_VALUE              "value"
#define KEY_REPLY_ID           "reply_id"

/**
Base command structure
*/
struct Command {
  CommandType                     type;
  k2eg::common::SerializationType serialization;
  std::string                     reply_topic;
  std::string                     reply_id;
};
DEFINE_PTR_TYPES(Command)

typedef std::vector<ConstCommandShrdPtr> ConstCommandShrdPtrVec;

static void
tag_invoke(boost::json::value_from_tag, boost::json::value &jv, CommandType const &cfg) {
  jv = {{"type", command_type_to_string(cfg)}};
}

static void
tag_invoke(boost::json::value_from_tag, boost::json::value &jv, Command const &c) {
  jv = {
    {"serialization", serialization_to_string(c.serialization)}, 
    {"reply_topic", c.reply_topic},
    {"reply_id", c.reply_id}
  };
}

}  // namespace k2eg::controller::command::cmd

#endif  // K2EG_CONTROLLER_COMMAND_CMD_COMMAND_H_