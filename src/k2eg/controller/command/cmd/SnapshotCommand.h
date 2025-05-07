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
    "reply_id", "reply_id",
    "serialization", "json|msgpack",
    "is_continuous", true|false,
    "repeat_delay_msec", 1000,
    "time_window_msec", 1000,
    "snapshot_name", "snapshot_name"
}
*/
struct SnapshotCommand : public Command {
  // the snapshot is repeating
  bool is_continuous;
  // the list of PV to be monitored
  std::vector<std::string> pv_name_list;
  // the repeat delay after the last snapshot
  std::int32_t repeat_delay_msec;
  // the time window to collect data
  std::int32_t time_window_msec;
  // the name of the snapshot
  std::string snapshot_name;
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