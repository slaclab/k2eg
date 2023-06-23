#ifndef K2EG_CONTROLLER_COMMAND_CMD_GETCOMMAND_H_
#define K2EG_CONTROLLER_COMMAND_CMD_GETCOMMAND_H_

#include <k2eg/controller/command/cmd/Command.h>
namespace k2eg::controller::command::cmd {
/**
 *     {
        "command", "get",
        "protocol", "pva|ca",
        "pv_name", "channel::a",
        "dest_topic", "destination_topic"
        }
*/
struct GetCommand : public Command {
  std::string destination_topic;
  std::string reply_id;
};
DEFINE_PTR_TYPES(GetCommand)
static void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, GetCommand const& c) {
  jv = {{"serialization", c.serialization},
        {"pv_name", c.pv_name},
        {"protocol", c.protocol},
        {"destination_topic", c.destination_topic},
        {"reply_id", c.reply_id}};
}
}  // namespace k2eg::controller::command::cmd

#endif  // K2EG_CONTROLLER_COMMAND_CMD_GETCOMMAND_H_