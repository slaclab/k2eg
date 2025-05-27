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

enum class SnapshotType
{
    unknown,
    // is the default type, that represent a snapshot that when is triggered (automatically or manually)
    // will be publish the last arrived data fropm all pvs
    NORMAL,
    // When triggered, publishes all values received from all PVs within the time window before the trigger.
    TIMED_BUFFERED
};

constexpr const char* snapshot_type_to_string(SnapshotType t) noexcept
{
    switch (t)
    {
    case SnapshotType::NORMAL: return "Normal";
    case SnapshotType::TIMED_BUFFERED: return "TimedBuffered";
    case SnapshotType::unknown: return "Unknown";
    }
    return "Unknown";
}

constexpr SnapshotType snapshot_type_from_string(const char* st) noexcept
{
    if (st == nullptr)
        return SnapshotType::unknown;

    // Compare case-insensitively
    auto iequals = [](const char* a, const char* b)
    {
        while (*a && *b)
        {
            if (std::tolower(static_cast<unsigned char>(*a)) != std::tolower(static_cast<unsigned char>(*b)))
                return false;
            ++a;
            ++b;
        }
        return *a == *b;
    };

    if (iequals(st, "normal"))
        return SnapshotType::NORMAL;
    if (iequals(st, "timedbuffered"))
        return SnapshotType::TIMED_BUFFERED;
    if (iequals(st, "unknown"))
        return SnapshotType::unknown;
    return SnapshotType::unknown;
}

inline std::ostream& operator<<(std::ostream& os, const SnapshotType& type)
{
    return os << snapshot_type_to_string(type);
}

static void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, SnapshotType const& cfg)
{
    jv = {{"type", snapshot_type_to_string(cfg)}};
}

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
    // define the type of snapshot
    SnapshotType type = SnapshotType::NORMAL;
    // the fields to include in the snapshot for each pv data
    std::unordered_set<std::string> pv_field_filter_list = {};
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
      {"triggered", c.triggered},
      {"type", snapshot_type_to_string(c.type)},
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