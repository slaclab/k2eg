#ifndef K2EG_CONTROLLER_COMMAND_CMD_MONITORCOMMAND_H_
#define K2EG_CONTROLLER_COMMAND_CMD_MONITORCOMMAND_H_

#include <k2eg/controller/command/cmd/Command.h>
namespace k2eg::controller::command::cmd {
/**
 *     {
        "command", "monitor",
        "activate", true|false,
        "protocol", "pva|ca",
        "pv_name", "channel::a",
        "reply_topic", "reply_topic"
        }
*/
struct MonitorCommand : public Command {
    bool activate;
    std::string monitor_destination_topic;
    std::string reply_id;
    std::string reply_topic;
};
DEFINE_PTR_TYPES(MonitorCommand)

static void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, MonitorCommand const& c) {
    jv = {
        {"serialization", serialization_to_string(c.serialization)},
        {"pv_name", c.pv_name}, 
        {"protocol", c.protocol},
        {"activate", c.activate}, 
        {"reply_topic", c.reply_topic},
        {"monitor_destination_topic", c.monitor_destination_topic},
        {"reply_id", c.reply_id}
        };
}
} // namespace k2eg::controller::command::cmd
#endif // K2EG_CONTROLLER_COMMAND_CMD_MONITORCOMMAND_H_