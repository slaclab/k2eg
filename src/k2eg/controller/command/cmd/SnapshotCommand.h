#ifndef K2EG_CONTROLLER_COMMAND_CMD_SNAPSHOTCOMMAND_H_
#define K2EG_CONTROLLER_COMMAND_CMD_SNAPSHOTCOMMAND_H_

#include "k2eg/common/BaseSerialization.h"
#include <k2eg/controller/command/cmd/Command.h>
#include <unordered_set>

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
    std::unordered_set<std::string> pv_name_list;
    // the time window to collect data
    std::int32_t time_window_msec;

    SnapshotCommand()
        : Command(CommandType::snapshot)
        , time_window_msec(0) {}

    SnapshotCommand(k2eg::common::SerializationType serialization, const std::string& reply_topic, const std::string& reply_id, const std::unordered_set<std::string>& pv_name_list, std::int32_t time_window_msec)
        : Command(CommandType::snapshot, serialization, reply_topic, reply_id)
        , pv_name_list(pv_name_list)
        , time_window_msec(time_window_msec)
    {
    }
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
    std::unordered_set<std::string> pv_name_list;
    // the repeat delay after the last snapshot
    std::int32_t repeat_delay_msec;
    // the time window to collect data
    std::int32_t time_window_msec;
    // if > 0 represent the sub time window used to push partial alredy taken data to the subscriber
    std::int32_t sub_push_delay_msec = 0; // delay to push the data to the subscriber
    // identify a triggered snapshot
    // if true the snapshot is triggered by the user
    // if false the snapshot is triggered by the repeating snapshot
    // the default is false
    bool triggered;
    // define the type of snapshot
    SnapshotType snapshot_type = SnapshotType::NORMAL;
    // the fields to include in the snapshot for each pv data
    std::unordered_set<std::string> pv_field_filter_list = {};

    RepeatingSnapshotCommand()
        : Command(CommandType::repeating_snapshot)
        , repeat_delay_msec(0)
        , time_window_msec(0)
        , triggered(false) {}

    RepeatingSnapshotCommand(
        k2eg::common::SerializationType serialization, 
        const std::string& reply_topic, 
        const std::string& reply_id, 
        const std::string& snapshot_name, 
        const std::unordered_set<std::string>& pv_name_list, 
        std::int32_t repeat_delay_msec, 
        std::int32_t time_window_msec, 
        std::int32_t sub_push_delay_msec, 
        bool triggered, 
        SnapshotType snapshot_type = SnapshotType::NORMAL, 
        const std::unordered_set<std::string>& pv_field_filter_list = {})
        : Command(CommandType::repeating_snapshot, serialization, reply_topic, reply_id)
        , snapshot_name(snapshot_name)
        , pv_name_list(pv_name_list)
        , repeat_delay_msec(repeat_delay_msec)
        , time_window_msec(time_window_msec)
        , sub_push_delay_msec(sub_push_delay_msec)
        , triggered(triggered)
        , snapshot_type(snapshot_type)
        , pv_field_filter_list(pv_field_filter_list)
    {
    }
};
DEFINE_PTR_TYPES(RepeatingSnapshotCommand)

/*
@brief Convert a JSON string to a RepeatingSnapshotCommand object
@param json_string The JSON string to convert.
@param cmd The RepeatingSnapshotCommand object to populate.
This function parses the JSON string and fills the cmd object with the corresponding values.
If the JSON string is not valid or does not contain the expected fields, the cmd object will
remain unchanged.
*/
static void from_json(const std::string& json_string, RepeatingSnapshotCommand& cmd)
{
    boost::json::value json_value = boost::json::parse(json_string);
    if (!json_value.is_object())
        return;

    const auto& obj = json_value.as_object();
    // cmd.type = CommandType::repeating_snapshot;
    cmd.reply_id = obj.contains(KEY_REPLY_ID) ? obj.at(KEY_REPLY_ID).as_string() : "";
    cmd.reply_topic = obj.contains(KEY_REPLY_TOPIC) ? obj.at(KEY_REPLY_TOPIC).as_string() : "";
    cmd.serialization = common::serialization_from_string((obj.contains(KEY_SERIALIZATION) ? std::string(obj.at(KEY_SERIALIZATION).as_string()) : std::string("unknown")));
    cmd.pv_name_list = std::unordered_set<std::string>{};
    if (obj.contains(KEY_PV_NAME_LIST))
    {
        const auto& pv_list = obj.at(KEY_PV_NAME_LIST).as_array();
        for (const auto& pv : pv_list)
        {
            if (pv.is_string())
                cmd.pv_name_list.insert(std::string(pv.as_string()));
        }
    }
    cmd.repeat_delay_msec = obj.contains(KEY_REPEAT_DELAY_MSEC) ? obj.at(KEY_REPEAT_DELAY_MSEC).as_int64() : 0;
    cmd.time_window_msec = obj.contains(KEY_TIME_WINDOW_MSEC) ? obj.at(KEY_TIME_WINDOW_MSEC).as_int64() : 0;
    cmd.snapshot_name = obj.contains(KEY_SNAPSHOT_NAME) ? std::string(obj.at(KEY_SNAPSHOT_NAME).as_string()) : "";
    cmd.sub_push_delay_msec = obj.contains(KEY_SUB_PUSH_DELAY_MSEC) ? obj.at(KEY_SUB_PUSH_DELAY_MSEC).as_int64() : 0;
    cmd.triggered = obj.contains(KEY_TRIGGERED) ? obj.at(KEY_TRIGGERED).as_bool() : false;
    cmd.pv_field_filter_list = std::unordered_set<std::string>{};
    if (obj.contains(KEY_PV_FIELD_FILTER_LIST))
    {
        const auto& field_list = obj.at(KEY_PV_FIELD_FILTER_LIST).as_array();
        for (const auto& field : field_list)
        {
            if (field.is_string())
                cmd.pv_field_filter_list.insert(std::string(field.as_string()));
        }
    }
    cmd.snapshot_type = SnapshotType::NORMAL; // Default type if not specified
    if (obj.contains(KEY_SNAPSHOT_TYPE))
    {
        cmd.snapshot_type = snapshot_type_from_string(obj.at(KEY_SNAPSHOT_TYPE).as_string().c_str());
    }
}

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

    RepeatingSnapshotStopCommand()
        : Command(CommandType::repeating_snapshot_stop) {}

    RepeatingSnapshotStopCommand(k2eg::common::SerializationType serialization, std::string reply_topic, std::string reply_id, const std::string& snapshot_name)
        : Command(CommandType::repeating_snapshot_stop, serialization, reply_topic, reply_id)
        , snapshot_name(snapshot_name)
    {
    }
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

    RepeatingSnapshotTriggerCommand()
        : Command(CommandType::repeating_snapshot_trigger) {}

    RepeatingSnapshotTriggerCommand(k2eg::common::SerializationType serialization, std::string reply_topic, std::string reply_id, const std::string& snapshot_name, const std::map<std::string, std::string>& tags = {})
        : Command(CommandType::repeating_snapshot_trigger, serialization, reply_topic, reply_id)
        , snapshot_name(snapshot_name)
        , tags(tags)
    {
    }
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
      {"type", command_type_to_string(CommandType::repeating_snapshot)},
      {"serialization", serialization_to_string(c.serialization)}, 
      {"reply_id", c.reply_id}, 
      {"reply_topic", c.reply_topic}, 
      {"snapshot_name", c.snapshot_name}, 
      {"pv_name_list", std::move(pv_array)}, 
      {"repeat_delay_msec", c.repeat_delay_msec}, 
      {"time_window_msec", c.time_window_msec},
      {"sub_push_delay_msec", c.sub_push_delay_msec},
      {"pv_field_filter_list", boost::json::array(c.pv_field_filter_list.begin(), c.pv_field_filter_list.end())},
      {"triggered", c.triggered},
      {"snapshot_type", snapshot_type_to_string(c.snapshot_type)},
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