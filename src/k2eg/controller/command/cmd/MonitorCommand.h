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
        "dest_topic", "destination_topic"
        }
*/
struct MonitorCommand : public Command {
    bool activate;
    std::string destination_topic;
};
DEFINE_PTR_TYPES(MonitorCommand)

static void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, MonitorCommand const& c) {
    jv = {
        {"serialization", serialization_to_string(c.serialization)},
        {"pv_name", c.pv_name}, 
        {"protocol", c.protocol},
        {"activate", c.activate}, 
        {"destination_topic", c.destination_topic}
        };
}
} // namespace k2eg::controller::command::cmd
#endif // K2EG_CONTROLLER_COMMAND_CMD_MONITORCOMMAND_H_