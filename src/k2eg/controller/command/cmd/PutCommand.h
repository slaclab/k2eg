#ifndef K2EG_CONTROLLER_COMMAND_CMD_PUTCOMMAND_H_
#define K2EG_CONTROLLER_COMMAND_CMD_PUTCOMMAND_H_

#include <k2eg/controller/command/cmd/Command.h>
namespace k2eg::controller::command::cmd {

/**
 *     {
        "command", "put",
        "protocol", "pva|ca",
        "pv_name", "channel::a"
        "value", value"
        }
*/
struct PutCommand : public Command {
  std::string pv_name;
  std::string value;
};

DEFINE_PTR_TYPES(PutCommand)
static void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, PutCommand const& c) {
  jv = {{"serialization", c.serialization},
        {"pv_name", c.pv_name},
        {"protocol", c.protocol},
        {"reply_topic", c.reply_topic},
        {"value", c.value},
        {"reply_id", c.reply_id}};
}
}  // namespace k2eg::controller::command::cmd
#endif  // K2EG_CONTROLLER_COMMAND_CMD_PUTCOMMAND_H_