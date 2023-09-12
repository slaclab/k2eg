
#ifndef k2eg_CONTROLLER_COMMAND_CMDOPCODE_H_
#define k2eg_CONTROLLER_COMMAND_CMDOPCODE_H_

#include <k2eg/common/types.h>
#include <k2eg/controller/command/cmd/Command.h>
#include <k2eg/controller/command/cmd/GetCommand.h>
#include <k2eg/controller/command/cmd/InfoCommand.h>
#include <k2eg/controller/command/cmd/MonitorCommand.h>
#include <k2eg/controller/command/cmd/PutCommand.h>

#include <any>
#include <boost/json.hpp>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "boost/json/object.hpp"
#include "boost/json/value.hpp"
#include "k2eg/service/log/ILogger.h"

namespace k2eg::controller::command {
#define JSON_VALUE_TO(t, v) boost::json::value_to<t>(v)

DEFINE_MAP_FOR_TYPE(std::string, std::any, FieldValuesMap)
typedef std::unique_ptr<FieldValuesMap> FieldValuesMapUPtr;

#define BOOST_JSON_TO_STRIN(t, x) boost::json::serialize(boost::json::value_from(*static_pointer_cast<const t>(c)));

static const std::string
to_json_string(cmd::ConstCommandShrdPtr c) {
  switch (c->type) {
    case cmd::CommandType::get: return BOOST_JSON_TO_STRIN(cmd::GetCommand, c);
    case cmd::CommandType::info: return BOOST_JSON_TO_STRIN(cmd::InfoCommand, c);
    case cmd::CommandType::monitor: return BOOST_JSON_TO_STRIN(cmd::MonitorCommand, c);
    case cmd::CommandType::put: return BOOST_JSON_TO_STRIN(cmd::PutCommand, c);
    case cmd::CommandType::unknown: return "Unknown";
  }
  return "Unknown";
}

inline std::string
check_reply_topic(const boost::json::object& o, k2eg::service::log::ILoggerShrdPtr l) {
  std::string reply_topic;
  if (auto v = o.if_contains(KEY_REPLY_TOPIC)) {
    if (v->is_string()) {
      reply_topic = v->as_string();
    } else {
      l->logMessage("Destination topic is need to be string", service::log::LogLevel::ERROR);
    }
  }
  return reply_topic;
}


inline std::string
check_for_reply_id(const boost::json::object& o, k2eg::service::log::ILoggerShrdPtr l) {
  std::string rep_id;
  if (auto v = o.if_contains(KEY_REPLY_ID)) {
    if (v->is_string()) {
      rep_id = v->as_string();
    } else {
      l->logMessage("Reply id need to be string", service::log::LogLevel::ERROR);
    }
  }
  return rep_id;
}

inline common::SerializationType
check_for_serialization(const boost::json::object& o, common::SerializationType default_type, k2eg::service::log::ILoggerShrdPtr l) {
  common::SerializationType ser_type = default_type;
  if (auto v = o.if_contains(KEY_SERIALIZATION)) {
    if(!v->is_string()) {
        l->logMessage("The kery serialization need to be a string", service::log::LogLevel::ERROR);
        return ser_type;
    }
    auto cmd = v->as_string();
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), [](unsigned char c) { return std::tolower(c); });
    if (cmd.compare("json") == 0)
      ser_type = common::SerializationType::JSON;
    else if (cmd.compare("msgpack") == 0)
      ser_type = common::SerializationType::Msgpack;
    else if (cmd.compare("msgpack-compact") == 0)
      ser_type = common::SerializationType::MsgpackCompact;
  }
  return ser_type;
}

// Represent a pv with the record name
struct PVName {
    std::string pv_name;
    std::string field_name;
};
DEFINE_PTR_TYPES(PVName)

/**
 * class that help to map the json structure to a command
 */
class MapToCommand {
  static PVNameUPtr saintizePVName(const std::string& pv_name);
  /**
   * Extract the command type
   */
  static cmd::CommandType getCMDType(const boost::json::object& ob);
  /**
   * Verify the presence of all the filed within the json object
   */
  static FieldValuesMapUPtr checkFields(const boost::json::object& obj, const std::vector<std::tuple<std::string, boost::json::kind>>& fields);

 public:
  static cmd::ConstCommandShrdPtr parse(const boost::json::object& obj);
};

}  // namespace k2eg::controller::command

#endif  // k2eg_CONTROLLER_COMMAND_CMDOPCODE_H_
