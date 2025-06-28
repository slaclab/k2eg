#ifndef K2EG_CONTROLLER_COMMAND_CMD_MONITORCOMMAND_H_
#define K2EG_CONTROLLER_COMMAND_CMD_MONITORCOMMAND_H_

#include <k2eg/controller/command/cmd/Command.h>

namespace k2eg::controller::command::cmd {
/**
 *     {
        "command", "monitor",
        "activate", true|false,
        "pv_name", [pva|ca]<pv name>,
        "reply_topic", "reply_topic"
        }
*/
struct MonitorCommand : public Command {
  std::string pv_name;
  std::string monitor_destination_topic;
};
DEFINE_PTR_TYPES(MonitorCommand)

static void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, MonitorCommand const& c) {
  jv = {{"serialization", serialization_to_string(c.serialization)},
        {"pv_name", c.pv_name},
        {"reply_id", c.reply_id},
        {"reply_topic", c.reply_topic},
        {"monitor_destination_topic", c.monitor_destination_topic}};
}

// automatic start monitor for multiple pv
struct MultiMonitorCommand : public Command {
  std::unordered_set<std::string> pv_name_list;
};
DEFINE_PTR_TYPES(MultiMonitorCommand)
static void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, MultiMonitorCommand const& c) {
  boost::json::array pv_array;
  for (const auto& name : c.pv_name_list) { pv_array.emplace_back(name); }

  jv = {{"serialization", serialization_to_string(c.serialization)},
        {"pv_name_list", std::move(pv_array)},
        {"reply_id", c.reply_id},
        {"reply_topic", c.reply_topic}};
}

}  // namespace k2eg::controller::command::cmd
#endif  // K2EG_CONTROLLER_COMMAND_CMD_MONITORCOMMAND_H_