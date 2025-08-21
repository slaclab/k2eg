#ifndef K2EG_CONTROLLER_COMMAND_CMD_COMMAND_H_
#define K2EG_CONTROLLER_COMMAND_CMD_COMMAND_H_

#include <k2eg/common/serialization.h>
#include <k2eg/common/types.h>

#include <boost/json.hpp>

#include <string>

namespace k2eg::controller::command::cmd {

#define KEY_COMMAND                 "command"
#define KEY_SERIALIZATION           "serialization"
#define KEY_TYPE                    "type"
#define KEY_PV_NAME                 "pv_name"
#define KEY_PV_NAME_LIST            "pv_name_list"
#define KEY_PV_FIELD_FILTER_LIST    "pv_field_filter_list"
#define KEY_REPLY_TOPIC             "reply_topic"
#define KEY_MONITOR_DEST_TOPIC      "monitor_dest_topic"
#define KEY_VALUE                   "value"
#define KEY_REPLY_ID                "reply_id"
#define KEY_SNAPSHOT_NAME           "snapshot_name"
#define KEY_SNAPSHOT_TYPE           "snapshot_type"
#define KEY_REPEAT_DELAY_MSEC       "repeat_delay_msec"
#define KEY_TIME_WINDOW_MSEC        "time_window_msec"
#define KEY_TRIGGERED               "triggered"
#define KEY_TAGS                    "tags"
#define KEY_SUB_PUSH_DELAY_MSEC     "sub_push_delay_msec"


/**
 * @enum CommandType
 * @brief Enumerates all possible command types supported by the controller.
 *
 * - monitor: Start monitoring a single PV.
 * - multi_monitor: Start monitoring multiple PVs.
 * - get: Retrieve the value of a PV.
 * - put: Set the value of a PV.
 * - info: Get information about a PV.
 * - snapshot: Take a snapshot of PV values.
 * - repeating_snapshot: Take snapshots at regular intervals.
 * - repeating_snapshot_stop: Stop a repeating snapshot operation.
 * - repeating_snapshot_trigger: Trigger a repeating snapshot manually.
 * - unknown: Unknown or unsupported command type.
 */
enum class CommandType
{
    monitor,                  /**< Start monitoring a single PV. */
    multi_monitor,            /**< Start monitoring multiple PVs. */
    get,                      /**< Retrieve the value of a PV. */
    put,                      /**< Set the value of a PV. */
    info,                     /**< Get information about a PV. */
    snapshot,                 /**< Take a snapshot of PV values. */
    repeating_snapshot,       /**< Take snapshots at regular intervals. */
    repeating_snapshot_stop,  /**< Stop a repeating snapshot operation. */
    repeating_snapshot_trigger, /**< Trigger a repeating snapshot manually. */
    unknown                   /**< Unknown or unsupported command type. */
};


/**
 * @brief Converts a CommandType enum value to its string representation.
 *
 * @param t The CommandType value.
 * @return The corresponding string name for the command type.
 */
constexpr const char* command_type_to_string(CommandType t) noexcept
{
    switch (t)
    {
    case CommandType::get: return "get";
    case CommandType::info: return "info";
    case CommandType::monitor: return "monitor";
    case CommandType::multi_monitor: return "multi-monitor";
    case CommandType::put: return "put";
    case CommandType::snapshot: return "snapshot";
    case CommandType::repeating_snapshot: return "repeating_snapshot";
    case CommandType::repeating_snapshot_stop: return "repeating_snapshot_stop";
    case CommandType::repeating_snapshot_trigger: return "repeating_snapshot_trigger";
    case CommandType::unknown: return "unknown";
    }
    return "undefined";
}


/**
 * @brief Stream output operator for CommandType.
 *
 * Allows CommandType to be printed directly to output streams.
 *
 * @param os Output stream.
 * @param type CommandType value.
 * @return Reference to the output stream.
 */
inline std::ostream& operator<<(std::ostream& os, const k2eg::controller::command::cmd::CommandType& type)
{
    return os << command_type_to_string(type);
}


/**
 * @struct Command
 * @brief Represents a generic command sent to the controller.
 *
 * Contains the command type, serialization format, reply topic, and reply ID.
 */
struct Command
{
    CommandType                     type;           /**< Type of the command. */
    k2eg::common::SerializationType serialization;  /**< Serialization format for the command. */
    std::string                     reply_topic;    /**< Topic to send replies to. */
    std::string                     reply_id;       /**< Unique identifier for the reply. */

    /**
     * @brief Default constructor.
     */
    Command() = default;

    /**
     * @brief Constructor with command type only.
     * @param type The type of command.
     */
    Command(CommandType type)
        : type(type)
    {
    }

    /**
     * @brief Full constructor.
     * @param type The type of command.
     * @param serialization Serialization format.
     * @param reply_topic Topic for replies.
     * @param reply_id Reply identifier.
     */
    Command(CommandType type, k2eg::common::SerializationType serialization, std::string reply_topic, std::string reply_id)
        : type(type), serialization(serialization), reply_topic(std::move(reply_topic)), reply_id(std::move(reply_id))
    {
    }
};


/**
 * @brief Defines shared and unique pointer types for Command.
 */
DEFINE_PTR_TYPES(Command)


/**
 * @brief Vector of constant shared pointers to Command objects.
 */
typedef std::vector<ConstCommandShrdPtr> ConstCommandShrdPtrVec;


/**
 * @brief Serializes CommandType to JSON using Boost.JSON.
 *
 * @param jv JSON value to populate.
 * @param cfg CommandType to serialize.
 */
static void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, CommandType const& cfg)
{
    jv = {{"type", command_type_to_string(cfg)}};
}


/**
 * @brief Serializes Command to JSON using Boost.JSON.
 *
 * @param jv JSON value to populate.
 * @param c Command to serialize.
 */
static void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Command const& c)
{
    jv = {{"serialization", serialization_to_string(c.serialization)}, {"reply_topic", c.reply_topic}, {"reply_id", c.reply_id}};
}

} // namespace k2eg::controller::command::cmd

#endif // K2EG_CONTROLLER_COMMAND_CMD_COMMAND_H_