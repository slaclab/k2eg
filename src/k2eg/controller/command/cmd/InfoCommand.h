#ifndef K2EG_CONTROLLER_COMMAND_CMD_INFOCOMMAND_H_
#define K2EG_CONTROLLER_COMMAND_CMD_INFOCOMMAND_H_

#include <k2eg/controller/command/cmd/Command.h>
namespace k2eg::controller::command::cmd {

/**
 *     {
        "command", "info",
        "protocol", "pva|ca",
        "pv_name", "channel::a",
        "dest_topic", "destination_topic"
        }
*/

struct InfoCommand : public Command {
    std::string destination_topic;
};
DEFINE_PTR_TYPES(InfoCommand)


static void tag_invoke(boost::json::value_from_tag, boost::json::value &jv,  InfoCommand const &c) {
 jv = {
        {"serialization", serialization_to_string(c.serialization)},
        {"pv_name", c.pv_name},
        {"protocol", c.protocol},
        {"destination_topic", c.destination_topic}
    };
}
}



#endif // K2EG_CONTROLLER_COMMAND_CMD_INFOCOMMAND_H_
