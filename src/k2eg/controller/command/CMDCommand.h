
#ifndef k2eg_CONTROLLER_COMMAND_CMDOPCODE_H_
#define k2eg_CONTROLLER_COMMAND_CMDOPCODE_H_

#include <k2eg/common/types.h>

#include <boost/json.hpp>

#include <any>
#include <boost/json.hpp>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace k2eg::controller::command {
#define JSON_VALUE_TO(t, v) boost::json::value_to<t>(v)

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

/**
 *     {
        "command", "monitor",
        "activate", true|false,
        "protocol", "pva|ca",
        "channel_name", "channel::a",
        "dest_topic", "destination_topic"
        }
*/
struct AquireCommand : public Command {
    bool activate;
    std::string destination_topic;
};
DEFINE_PTR_TYPES(AquireCommand)

/**
 *     {
        "command", "get",
        "protocol", "pva|ca",
        "channel_name", "channel::a",
        "dest_topic", "destination_topic"
        }
*/
struct GetCommand : public Command {
    std::string destination_topic;
};
DEFINE_PTR_TYPES(GetCommand)

/**
 *     {
        "command", "put",
        "protocol", "pva|ca",
        "channel_name", "channel::a"
        "value", value"
        }
*/
struct PutCommand : public Command {
    std::string value;
};
DEFINE_PTR_TYPES(PutCommand)

/**
 *     {
        "command", "info",
        "protocol", "pva|ca",
        "channel_name", "channel::a",
        "dest_topic", "destination_topic"
        }
*/
struct InfoCommand : public Command {
    std::string destination_topic;
};
DEFINE_PTR_TYPES(InfoCommand)

 static void tag_invoke(boost::json::value_from_tag, boost::json::value &jv, CommandType const &cfg) {
    jv = {
        {"type", command_type_to_string(cfg)}
    };
 }
 static void tag_invoke(boost::json::value_from_tag, boost::json::value &jv,  Command const &c) {
 jv = {
        {"channel_name", c.channel_name},
        {"protocol", c.protocol}
    };
}
static void tag_invoke(boost::json::value_from_tag, boost::json::value &jv,  AquireCommand const &c) {
 jv = {
        {"channel_name", c.channel_name},
        {"protocol", c.protocol},
        {"activate", c.activate},
        {"destination_topic", c.destination_topic}
    };
}
static void tag_invoke(boost::json::value_from_tag, boost::json::value &jv,  GetCommand const &c) {
 jv = {
        {"channel_name", c.channel_name},
        {"protocol", c.protocol},
        {"destination_topic", c.destination_topic}
    };
}
static void tag_invoke(boost::json::value_from_tag, boost::json::value &jv,  PutCommand const &c) {
 jv = {
        {"channel_name", c.channel_name},
        {"protocol", c.protocol},
        {"value", c.value}
    };
}
static void tag_invoke(boost::json::value_from_tag, boost::json::value &jv,  InfoCommand const &c) {
 jv = {
        {"channel_name", c.channel_name},
        {"protocol", c.protocol},
        {"destination_topic", c.destination_topic}
    };
}

typedef std::shared_ptr<const Command> CommandConstShrdPtr;
typedef std::vector<CommandConstShrdPtr> CommandConstShrdPtrVec;

DEFINE_MAP_FOR_TYPE(std::string, std::any, FieldValuesMap)
typedef std::unique_ptr<FieldValuesMap> FieldValuesMapUPtr;

#define BOOST_JSON_TO_STRIN(t, x) \
boost::json::serialize(boost::json::value_from( \
            *static_pointer_cast<const t>(c) \
        ));

static const std::string to_json_string(CommandConstShrdPtr c) {
    switch(c->type) {
    case CommandType::get:
        return BOOST_JSON_TO_STRIN(GetCommand, c);
    case CommandType::info:
        return BOOST_JSON_TO_STRIN(InfoCommand, c);
    case CommandType::monitor:
        return BOOST_JSON_TO_STRIN(AquireCommand, c);
    case CommandType::put:
        return BOOST_JSON_TO_STRIN(PutCommand, c);
    case CommandType::unknown:
        return "Unknown";
    }
    return "Unknown";
}


/**
 * class that help to map the json structure to a command
 */
class MapToCommand {
    /**
     * Extract the command type
     */
    static CommandType getCMDType(const boost::json::object& ob);
    /**
     * Verify the presence of all the filed within the json object
     */
    static FieldValuesMapUPtr checkFields(const boost::json::object& obj,
                                          const std::vector<std::tuple<std::string, boost::json::kind>>& fields);

public:
    static CommandConstShrdPtr parse(const boost::json::object& obj);
};
} // namespace k2eg::controller::command

#endif // k2eg_CONTROLLER_COMMAND_CMDOPCODE_H_
