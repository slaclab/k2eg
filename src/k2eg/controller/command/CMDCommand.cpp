#include <k2eg/controller/command/CMDCommand.h>
#include <k2eg/service/ServiceResolver.h>
#include <k2eg/service/log/ILogger.h>

#include <vector>

#include "boost/json/array.hpp"
#include "k2eg/controller/command/cmd/Command.h"
#include "k2eg/controller/command/cmd/MonitorCommand.h"

using namespace k2eg::common;
using namespace k2eg::controller::command;
using namespace k2eg::controller::command::cmd;
using namespace k2eg::service;
using namespace k2eg::service::log;
using namespace boost::json;

CommandType
MapToCommand::getCMDType(const object& obj) {
  if (auto v = obj.if_contains(KEY_COMMAND)) {
    const auto cmd = v->as_string();
    if (cmd.compare("monitor") == 0)
      return CommandType::monitor;
    else if (cmd.compare("get") == 0)
      return CommandType::get;
    else if (cmd.compare("put") == 0)
      return CommandType::put;
    else if (cmd.compare("info") == 0)
      return CommandType::info;
  }
  return CommandType::unknown;
}

FieldValuesMapUPtr
MapToCommand::checkFields(const object& obj, const std::vector<std::tuple<std::string, kind>>& fields) {
  FieldValuesMapUPtr result;
  FieldValuesMapUPtr result_tmp = std::make_unique<FieldValuesMap>();
  std::for_each(begin(fields), end(fields), [&obj = obj, &result = result_tmp](const auto field) {
    if (obj.if_contains(std::get<0>(field)) != nullptr) {
      auto v = obj.find(std::get<0>(field))->value();
      if (v.kind() == std::get<1>(field)) {
        switch (v.kind()) {
          case kind::int64: result->insert(FieldValuesMapPair(std::get<0>(field), value_to<int64_t>(v))); break;
          case kind::bool_: result->insert(FieldValuesMapPair(std::get<0>(field), value_to<bool>(v))); break;
          case kind::uint64: result->insert(FieldValuesMapPair(std::get<0>(field), value_to<uint64_t>(v))); break;
          case kind::double_: result->insert(FieldValuesMapPair(std::get<0>(field), value_to<double>(v))); break;
          case kind::string: result->insert(FieldValuesMapPair(std::get<0>(field), value_to<std::string>(v))); break;
          case kind::array: result->insert(FieldValuesMapPair(std::get<0>(field), value_to<boost::json::array>(v))); break;
          default: break;
        }
      }
    }
  });
  if (fields.size() == result_tmp->size()) {
    // we have faound all the key that we need
    result = std::move(result_tmp);
  }
  return result;
}

ConstCommandShrdPtr
MapToCommand::parse(const object& obj) {
  auto logger = ServiceResolver<ILogger>::resolve();
#ifdef __DEBUG__
  logger->logMessage("Received command: " + serialize(obj), LogLevel::DEBUG);
#endif
  ConstCommandShrdPtr result = nullptr;
  switch (getCMDType(obj)) {
    case CommandType::monitor: {
      if (auto activation = obj.if_contains(KEY_ACTIVATE); activation != nullptr && activation->is_bool()) {
        const std::string       reply_id    = check_for_reply_id(obj, logger);
        const std::string       reply_topic = check_reply_topic(obj, logger);
        const SerializationType ser_type    = check_for_serialization(obj, SerializationType::JSON, logger);
        std::string             event_destination_topic;
        if (auto v = obj.if_contains(KEY_MONITOR_DEST_TOPIC); v != nullptr && v->is_string()) {
          event_destination_topic = v->as_string();
        } else {
          // by default uses the reply topic as monitor event
          event_destination_topic = reply_topic;
        }

        if (activation->as_bool()) {
          if (!event_destination_topic.empty()) {
            if (auto fields = checkFields(obj, {{KEY_PROTOCOL, kind::string}, {KEY_PV_NAME, kind::string}}); fields != nullptr) {
              result = std::make_shared<MonitorCommand>(MonitorCommand{CommandType::monitor,
                                                                       ser_type,
                                                                       reply_topic,
                                                                       reply_id,
                                                                       std::any_cast<std::string>(fields->find(KEY_PROTOCOL)->second),
                                                                       std::any_cast<std::string>(fields->find(KEY_PV_NAME)->second),
                                                                       true,

                                                                       event_destination_topic});
            } else {
              logger->logMessage("Missing key for the AquireCommand: " + serialize(obj), LogLevel::ERROR);
            }
          } else {
            logger->logMessage("Impossible to determinate the event destination topic: " + serialize(obj), LogLevel::ERROR);
          }
        } else {
          if (auto fields = checkFields(obj, {{KEY_PV_NAME, kind::string}}); fields != nullptr) {
            result = std::make_shared<MonitorCommand>(MonitorCommand{CommandType::monitor,
                                                                     ser_type,
                                                                     reply_topic,
                                                                     reply_id,
                                                                     "",
                                                                     std::any_cast<std::string>(fields->find(KEY_PV_NAME)->second),
                                                                     false,
                                                                     event_destination_topic});
          } else {
            logger->logMessage("Missing key for the AquireCommand: " + serialize(obj), LogLevel::ERROR);
          }
        }
      }
      break;
    }

    case CommandType::multi_monitor: {
      if (auto activation = obj.if_contains(KEY_ACTIVATE); activation != nullptr && activation->is_bool()) {
        const std::string       reply_id    = check_for_reply_id(obj, logger);
        const std::string       reply_topic = check_reply_topic(obj, logger);
        const SerializationType ser_type    = check_for_serialization(obj, SerializationType::JSON, logger);
        if (auto fields = checkFields(obj, {{KEY_PROTOCOL, kind::string}, {KEY_PV_NAME_LIST, kind::array}}); fields != nullptr) {
          std::vector<std::string> pv_name_list;
          auto                     json_array = std::any_cast<boost::json::array>(fields->find(KEY_PV_NAME_LIST)->second);
          // find all stirng in the vector
          for (auto& element : json_array) {
            if (element.kind() != kind::string) continue;
            pv_name_list.push_back(value_to<std::string>(element));
          }
          if (pv_name_list.size()) {
            // we can create the command
            result = std::make_shared<MultiMonitorCommand>(MultiMonitorCommand{
                CommandType::multi_monitor, ser_type, reply_topic, reply_id, std::any_cast<std::string>(fields->find(KEY_PROTOCOL)->second), pv_name_list});
          } else {
            logger->logMessage("The array should not be empty: " + serialize(obj), LogLevel::ERROR);
          }
        } else {
          logger->logMessage("Missing key for the AquireCommand: " + serialize(obj), LogLevel::ERROR);
        }
      }
      break;
    }

    case CommandType::get: {
      if (auto fields = checkFields(obj, {{KEY_PROTOCOL, kind::string}, {KEY_PV_NAME, kind::string}, {KEY_REPLY_TOPIC, kind::string}}); fields != nullptr) {
        std::string       reply_id    = check_for_reply_id(obj, logger);
        SerializationType ser_type    = check_for_serialization(obj, SerializationType::JSON, logger);
        const std::string reply_topic = check_reply_topic(obj, logger);
        result                        = std::make_shared<GetCommand>(GetCommand{CommandType::get,
                                                         ser_type,
                                                         reply_topic,
                                                         reply_id,
                                                         std::any_cast<std::string>(fields->find(KEY_PROTOCOL)->second),
                                                         std::any_cast<std::string>(fields->find(KEY_PV_NAME)->second)});
      } else {
        logger->logMessage("Missing key for the GetCommand: " + serialize(obj), LogLevel::ERROR);
      }
      break;
    }
    case CommandType::put: {
      if (auto fields = checkFields(obj, {{KEY_PROTOCOL, kind::string}, {KEY_PV_NAME, kind::string}, {KEY_VALUE, kind::string}}); fields != nullptr) {
        std::string       reply_id    = check_for_reply_id(obj, logger);
        SerializationType ser_type    = check_for_serialization(obj, SerializationType::JSON, logger);
        const std::string reply_topic = check_reply_topic(obj, logger);
        result                        = std::make_shared<PutCommand>(PutCommand{CommandType::put,
                                                         ser_type,
                                                         reply_topic,
                                                         reply_id,
                                                         std::any_cast<std::string>(fields->find(KEY_PROTOCOL)->second),
                                                         std::any_cast<std::string>(fields->find(KEY_PV_NAME)->second),
                                                         std::any_cast<std::string>(fields->find(KEY_VALUE)->second)});
      } else {
        logger->logMessage("Missing key for the PutCommand: " + serialize(obj), LogLevel::ERROR);
      }
      break;
    }
    case CommandType::info: {
      if (auto fields = checkFields(obj, {{KEY_PROTOCOL, kind::string}, {KEY_PV_NAME, kind::string}, {KEY_REPLY_TOPIC, kind::string}}); fields != nullptr) {
        std::string       reply_id    = check_for_reply_id(obj, logger);
        SerializationType ser_type    = check_for_serialization(obj, SerializationType::JSON, logger);
        const std::string reply_topic = check_reply_topic(obj, logger);
        result                        = std::make_shared<InfoCommand>(InfoCommand{CommandType::info,
                                                           ser_type,
                                                           reply_topic,
                                                           reply_id,
                                                           std::any_cast<std::string>(fields->find(KEY_PROTOCOL)->second),
                                                           std::any_cast<std::string>(fields->find(KEY_PV_NAME)->second)});
      } else {
        logger->logMessage("Missing key for the InfoCommand: " + serialize(obj), LogLevel::ERROR);
      }
      break;
    }
    case CommandType::unknown: {
      ServiceResolver<ILogger>::resolve()->logMessage("Command not found:" + serialize(obj), LogLevel::ERROR);
      break;
    }
  }
  return result;
}