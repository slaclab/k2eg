#ifndef K2EG_CONTROLLER_COMMAND_CMD_PUTCOMMAND_H_
#define K2EG_CONTROLLER_COMMAND_CMD_PUTCOMMAND_H_

#include <k2eg/controller/command/cmd/Command.h>

namespace k2eg::controller::command::cmd {

/**
 *     {
        "command", "put",
        "pv_name", "[pva|ca]<pv name>"
        "value", value"
        }
*/
struct PutCommand : public Command
{
    std::string pv_name;
    std::string value;
    PutCommand()
        : Command(CommandType::put) {};

    PutCommand(k2eg::common::SerializationType serialization, std::string reply_topic, std::string reply_id, const std::string& pv_name, const std::string& value)
        : Command(CommandType::put, serialization, reply_topic, reply_id)
        , pv_name(pv_name)
        , value(value)
    {
    }
};

DEFINE_PTR_TYPES(PutCommand)

static void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, PutCommand const& c)
{
    jv = {{"type", command_type_to_string(c.type)},
          {"serialization", serialization_to_string(c.serialization)},
          {"pv_name", c.pv_name},
          // {"protocol", c.protocol},
          {"reply_topic", c.reply_topic},
          {"value", c.value},
          {"reply_id", c.reply_id}};
}
} // namespace k2eg::controller::command::cmd
#endif // K2EG_CONTROLLER_COMMAND_CMD_PUTCOMMAND_H_