#include <k2eg/controller/command/CMDCommand.h>
#include <k2eg/service/ServiceResolver.h>
#include <k2eg/service/log/ILogger.h>

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

MessageSerType
MapToCommand::getSerializationType(const boost::json::value& v) {
  MessageSerType result = MessageSerType::unknown;
  auto           cmd    = v.as_string();
  std::transform(cmd.begin(), cmd.end(), cmd.begin(), [](unsigned char c) { return std::tolower(c); }  // correct
  );
  if (cmd.compare("json") == 0)
    result = MessageSerType::json;
  else if (cmd.compare("msgpack") == 0)
    result = MessageSerType::msgpack;
  else if (cmd.compare("msgpack-compact") == 0)
    result = MessageSerType::msgpack_compact;
  return result;
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
        if (activation->as_bool()) {
          if (auto fields = checkFields(obj, {{KEY_PROTOCOL, kind::string}, {KEY_CHANNEL_NAME, kind::string}, {KEY_DEST_TOPIC, kind::string}});
              fields != nullptr) {
            MessageSerType ser_type = MessageSerType::json;
            if (auto v = obj.if_contains(KEY_SERIALIZATION)) { ser_type = getSerializationType(*v); }
            result = std::make_shared<MonitorCommand>(MonitorCommand{CommandType::monitor,
                                                                     ser_type,
                                                                     std::any_cast<std::string>(fields->find(KEY_PROTOCOL)->second),
                                                                     std::any_cast<std::string>(fields->find(KEY_CHANNEL_NAME)->second),
                                                                     true,
                                                                     std::any_cast<std::string>(fields->find(KEY_DEST_TOPIC)->second)});
          } else {
            logger->logMessage("Missing key for the AquireCommand: " + serialize(obj), LogLevel::ERROR);
          }
        } else {
          if (auto fields = checkFields(obj, {{KEY_CHANNEL_NAME, kind::string}, {KEY_DEST_TOPIC, kind::string}}); fields != nullptr) {
            result = std::make_shared<MonitorCommand>(MonitorCommand{CommandType::monitor,
                                                                     MessageSerType::unknown,
                                                                     "",
                                                                     std::any_cast<std::string>(fields->find(KEY_CHANNEL_NAME)->second),
                                                                     false,
                                                                     std::any_cast<std::string>(fields->find(KEY_DEST_TOPIC)->second)});
          } else {
            logger->logMessage("Missing key for the AquireCommand: " + serialize(obj), LogLevel::ERROR);
          }
        }
      }
      break;
    }
    case CommandType::get: {
      if (auto fields = checkFields(obj, {{KEY_PROTOCOL, kind::string}, {KEY_CHANNEL_NAME, kind::string}, {KEY_DEST_TOPIC, kind::string}}); fields != nullptr) {
        MessageSerType ser_type = MessageSerType::json;
        if (auto v = obj.if_contains(KEY_SERIALIZATION)) { ser_type = getSerializationType(*v); }
        result = std::make_shared<GetCommand>(GetCommand{CommandType::get,
                                                         ser_type,
                                                         std::any_cast<std::string>(fields->find(KEY_PROTOCOL)->second),
                                                         std::any_cast<std::string>(fields->find(KEY_CHANNEL_NAME)->second),
                                                         std::any_cast<std::string>(fields->find(KEY_DEST_TOPIC)->second)});
      } else {
        logger->logMessage("Missing key for the GetCommand: " + serialize(obj), LogLevel::ERROR);
      }
      break;
    }
    case CommandType::put: {
      if (auto fields = checkFields(obj, {{KEY_PROTOCOL, kind::string}, {KEY_CHANNEL_NAME, kind::string}, {KEY_VALUE, kind::string}}); fields != nullptr) {
        MessageSerType ser_type = MessageSerType::json;
        result                  = std::make_shared<PutCommand>(PutCommand{CommandType::put,
                                                         ser_type,
                                                         std::any_cast<std::string>(fields->find(KEY_PROTOCOL)->second),
                                                         std::any_cast<std::string>(fields->find(KEY_CHANNEL_NAME)->second),
                                                         std::any_cast<std::string>(fields->find(KEY_VALUE)->second)});
      } else {
        logger->logMessage("Missing key for the PutCommand: " + serialize(obj), LogLevel::ERROR);
      }
      break;
    }
    case CommandType::info: {
      if (auto fields = checkFields(obj, {{KEY_PROTOCOL, kind::string}, {KEY_CHANNEL_NAME, kind::string}, {KEY_DEST_TOPIC, kind::string}}); fields != nullptr) {
        MessageSerType ser_type = MessageSerType::json;
        if (auto v = obj.if_contains(KEY_SERIALIZATION)) { ser_type = getSerializationType(*v); }
        result = std::make_shared<InfoCommand>(InfoCommand{CommandType::info,
                                                           ser_type,
                                                           std::any_cast<std::string>(fields->find(KEY_PROTOCOL)->second),
                                                           std::any_cast<std::string>(fields->find(KEY_CHANNEL_NAME)->second),
                                                           std::any_cast<std::string>(fields->find(KEY_DEST_TOPIC)->second)});
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