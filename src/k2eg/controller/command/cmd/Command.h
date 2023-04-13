#ifndef K2EG_CONTROLLER_COMMAND_CMD_COMMAND_H_
#define K2EG_CONTROLLER_COMMAND_CMD_COMMAND_H_

#include <string>
#include <boost/json.hpp>

namespace k2eg::controller::command::cmd {

// are all the possible command
enum class CommandType { monitor, get, put, info, unknown };
constexpr const char* command_type_to_string(CommandType t) noexcept {
    switch (t) {
    case CommandType::get:
        return "get";
    case CommandType::info:
        return "info";
    case CommandType::monitor:
        return "monitor";
    case CommandType::put:
        return "put";
    case CommandType::unknown:
        return "unknown";
    }
    return "undefined";
}

#define KEY_COMMAND "command"
#define KEY_SERIALIZATION "serialization"
#define KEY_PROTOCOL "protocol"
#define KEY_CHANNEL_NAME "channel_name"
#define KEY_ACTIVATE "activate"
#define KEY_DEST_TOPIC "dest_topic"
#define KEY_VALUE "value"

// is the type of the serialization
enum class MessageSerType: std::uint8_t{unknown , json, mesgpack};
constexpr const char* serialization_to_string(MessageSerType t) noexcept {
    switch (t) {
    case MessageSerType::json:
        return "json";
    case MessageSerType::mesgpack:
        return "mesgpack";
    case MessageSerType::unknown:
        return "unknown";
    }
    return "undefined";
}

struct Command {
    CommandType type;
    MessageSerType serialization;
    std::string protocol;
    std::string channel_name;
};
DEFINE_PTR_TYPES(Command)
typedef  std::vector<ConstCommandShrdPtr> ConstCommandShrdPtrVec;
 static void tag_invoke(boost::json::value_from_tag, boost::json::value &jv, CommandType const &cfg) {
    jv = {
        {"type", command_type_to_string(cfg)}
    };
 }

static void tag_invoke(boost::json::value_from_tag, boost::json::value &jv, MessageSerType const &ser) {
    jv = {
        {"type", serialization_to_string(ser)}
    };
 }

 static void tag_invoke(boost::json::value_from_tag, boost::json::value &jv,  Command const &c) {
 jv = {
        {"serialization", serialization_to_string(c.serialization)},
        {"channel_name", c.channel_name},
        {"protocol", c.protocol}

    };
}
}

#endif // K2EG_CONTROLLER_COMMAND_CMD_COMMAND_H_