#ifndef K2EG_CONTROLLER_COMMAND_CMD_SNAPSHOTCOMMAND_H_
#define K2EG_CONTROLLER_COMMAND_CMD_SNAPSHOTCOMMAND_H_

#include <k2eg/controller/command/cmd/Command.h>

namespace k2eg::controller::command::cmd {

/*
@brief Perform a snapshot of the current state of specific PV with determianted parameters
{
    "command", "snapshot",
    "pv_name_list", ["[pva|ca]<pv name>"", ...]
    "reply_topic", "reply_topic"
    "reply_id", "reply_id",
    "serialization", "json|msgpack",
    "time_window_msec", 1000
}
*/
struct SnapshotCommand : public Command
{
    // the list of PV to be monitored
    std::vector<std::string> pv_name_list;
    // the time window to collect data
    std::int32_t time_window_msec;
};
DEFINE_PTR_TYPES(SnapshotCommand)

/*
@brief Perform a repeating napshot of the current state of specific PV with determianted parameters
{
    "command", "repeating_snapshot",
    "pv_name_list", ["[pva|ca]<pv name>"", ...]
    "reply_topic", "reply_topic"
    "reply_id", "reply_id",
    "serialization", "json|msgpack",
    "repeat_delay_msec", 1000,
    "time_window_msec", 1000,
    "snapshot_name", "snapshot_name"
}
*/
struct RepeatingSnapshotCommand : public Command
{
    // the name of the snapshot
    std::string snapshot_name;
    // the list of PV to be monitored
    std::vector<std::string> pv_name_list;
    // the repeat delay after the last snapshot
    std::int32_t repeat_delay_msec;
    // the time window to collect data
    std::int32_t time_window_msec;
    // identify a triggered snapshot
    // if true the snapshot is triggered by the user
    // if false the snapshot is triggered by the repeating snapshot
    // the default is false
    bool triggered;
};
DEFINE_PTR_TYPES(RepeatingSnapshotCommand)

/*
@brief RepeatingSnapshotStopCommand is a command to stop the repeating snapshot
{
    "command", "repeating_snapshot_stop",
    "snapshot_name", "snapshot_name"
}
*/
struct RepeatingSnapshotStopCommand : public Command
{
    // the name of the snapshot
    std::string snapshot_name;
};
DEFINE_PTR_TYPES(RepeatingSnapshotStopCommand)

/*
@brief RepeatingSnapshotTriggerCommand is a command to trigger the repeating snapshot
{
    "command", "repeating_snapshot_trigger",
    "snapshot_name", "snapshot_name"
}
*/
struct RepeatingSnapshotTriggerCommand : public Command
{
    // the name of the snapshot to trigger
    std::string snapshot_name;
    // the tag to associate to the trigger
    std::map<std::string, std::string> tags = {};
};
DEFINE_PTR_TYPES(RepeatingSnapshotTriggerCommand)

// clang-format off
static boost::json::object map_to_json_object(const std::map<std::string, std::string>& m) {
    boost::json::object obj;
    for (const auto& [k, v] : m) {
        obj[k] = v;
    }
    return obj;
}

static void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, SnapshotCommand const& c)
{
    boost::json::array pv_array;
    for (const auto& name : c.pv_name_list)
    {
        pv_array.emplace_back(name);
    }

    jv = {
        {"serialization", serialization_to_string(c.serialization)},
        {"reply_id", c.reply_id},
        {"reply_topic", c.reply_topic},
        {"pv_name_list", std::move(pv_array)},
        {"time_window_msec", c.time_window_msec},
    };
}

static void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, RepeatingSnapshotCommand const& c)
{
    boost::json::array pv_array;
    for (const auto& name : c.pv_name_list)
    {
        pv_array.emplace_back(name);
    }
    jv = {
      {"serialization", serialization_to_string(c.serialization)}, 
      {"reply_id", c.reply_id}, 
      {"reply_topic", c.reply_topic}, 
      {"snapshot_name", c.snapshot_name}, 
      {"pv_name_list", std::move(pv_array)}, 
      {"repeat_delay_msec", c.repeat_delay_msec}, 
      {"time_window_msec", c.time_window_msec},
      {"triggered", c.triggered}
    };
}

static void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, RepeatingSnapshotStopCommand const& c)
{
    jv = {
        {"serialization", serialization_to_string(c.serialization)},
        {"reply_id", c.reply_id},
        {"reply_topic", c.reply_topic},
        {"snapshot_name", c.snapshot_name},
    };
}

static void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, RepeatingSnapshotTriggerCommand const& c)
{
    jv = {
        {"serialization", serialization_to_string(c.serialization)},
        {"reply_id", c.reply_id},
        {"reply_topic", c.reply_topic},
        {"snapshot_name", c.snapshot_name},
        {"tags", map_to_json_object(c.tags)},
    };
}

// clang-format on
} // namespace k2eg::controller::command::cmd
#endif // K2EG_CONTROLLER_COMMAND_CMD_SNAPSHOTCOMMAND_H_