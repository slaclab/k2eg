#ifndef K2EG_CONTROLLER_COMMAND_CMD_GETCOMMAND_H_
#define K2EG_CONTROLLER_COMMAND_CMD_GETCOMMAND_H_

#include <k2eg/controller/command/cmd/Command.h>
namespace k2eg::controller::command::cmd {
/**
 *     {
        "command", "get",
        "protocol", "pva|ca",
        "pv_name", "channel::a",
        "reply_topic", "reply_topic"
        }
*/
struct GetCommand : public Command {
  std::string pv_name;
};
DEFINE_PTR_TYPES(GetCommand)
static void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, GetCommand const& c) {
  jv = {{"serialization", serialization_to_string(c.serialization)},
        {"pv_name", c.pv_name},
        // {"protocol", c.protocol},
        {"reply_topic", c.reply_topic},
        {"reply_id", c.reply_id}};
}
}  // namespace k2eg::controller::command::cmd

#endif  // K2EG_CONTROLLER_COMMAND_CMD_GETCOMMAND_H_