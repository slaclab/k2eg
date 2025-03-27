#ifndef K2EG_CONTROLLER_COMMAND_CMD_SNAPSHOTCOMMAND_H_
#define K2EG_CONTROLLER_COMMAND_CMD_SNAPSHOTCOMMAND_H_

#include <k2eg/controller/command/cmd/Command.h>
namespace k2eg::controller::command::cmd {

/*
Perform a snapshot of the current state of specific PV with determianted parameters
{
    "command", "snapshot",
    "snapshot_id", "<user generated custom id>",
    "pv_name_list", ["[pva|ca]<pv name>"", ...]
    "reply_topic", "reply_topic"
    "reply_id", "reply_topic"
}
*/
struct SnapshotCommand : public Command {
  std::vector<std::string> pv_name_list;
};

DEFINE_PTR_TYPES(SnapshotCommand)

static void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, SnapshotCommand const& c) {
  boost::json::array pv_array;
  for (const auto& name : c.pv_name_list) { pv_array.emplace_back(name); }

  jv = {{"serialization", serialization_to_string(c.serialization)},
        {"pv_name_list", std::move(pv_array)},
        {"reply_id", c.reply_id},
        {"reply_topic", c.reply_topic}
    };
}
}  // namespace k2eg::controller::command::cmd
#endif  // K2EG_CONTROLLER_COMMAND_CMD_SNAPSHOTCOMMAND_H_