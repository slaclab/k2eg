#include <k2eg/controller/command/cmd/GetCommand.h>
#include <k2eg/controller/command/cmd/InfoCommand.h>
#include <k2eg/service/ServiceResolver.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/pubsub/IPublisher.h>

#include <k2eg/controller/command/CMDCommand.h>
#include <k2eg/controller/command/cmd/Command.h>
#include <k2eg/controller/command/cmd/MonitorCommand.h>
#include <k2eg/controller/command/cmd/SnapshotCommand.h>
#include <k2eg/controller/node/worker/CommandWorker.h>

#include <boost/json/array.hpp>

#include <cctype> // std::tolower
#include <vector>

using namespace k2eg::common;
using namespace k2eg::controller::command;
using namespace k2eg::controller::command::cmd;
using namespace k2eg::controller::node::worker;
using namespace k2eg::service;
using namespace k2eg::service::log;
using namespace boost::json;

// compare two char ignoring the case
bool ichar_equals(char a, char b)
{
    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
}

CommandType MapToCommand::getCMDType(const object& obj)
{
    if (auto v = obj.if_contains(KEY_COMMAND))
    {
        const auto cmd = v->as_string();
        if (cmd.compare("monitor") == 0)
            return CommandType::monitor;
        else if (cmd.compare("multi-monitor") == 0)
            return CommandType::multi_monitor;
        else if (cmd.compare("get") == 0)
            return CommandType::get;
        else if (cmd.compare("put") == 0)
            return CommandType::put;
        else if (cmd.compare("info") == 0)
            return CommandType::info;
        else if (cmd.compare("snapshot") == 0)
            return CommandType::snapshot;
        else if (cmd.compare("repeating_snapshot") == 0)
            return CommandType::repeating_snapshot;
        else if (cmd.compare("repeating_snapshot_stop") == 0)
            return CommandType::repeating_snapshot_stop;
        else if (cmd.compare("repeating_snapshot_trigger") == 0)
            return CommandType::repeating_snapshot_trigger;
    }
    return CommandType::unknown;
}

// check if the current object has the needed field (with types)
// if all required fields are found return a map with the key and the value
// otherwise return nullptr
FieldValuesMapUPtr MapToCommand::checkFields(const object& obj, const std::vector<std::tuple<std::string, kind>>& required_fields)
{
    FieldValuesMapUPtr result;
    FieldValuesMapUPtr result_tmp = std::make_unique<FieldValuesMap>();
    std::for_each(begin(required_fields), end(required_fields),
                  [&obj = obj, &result = result_tmp](const auto field)
                  {
                      if (obj.if_contains(std::get<0>(field)) != nullptr)
                      {
                          auto v = obj.find(std::get<0>(field))->value();
                          if (v.kind() == std::get<1>(field))
                          {
                              switch (v.kind())
                              {
                              case kind::int64:
                                  result->insert(FieldValuesMapPair(std::get<0>(field), value_to<int64_t>(v)));
                                  break;
                              case kind::bool_:
                                  result->insert(FieldValuesMapPair(std::get<0>(field), value_to<bool>(v)));
                                  break;
                              case kind::uint64:
                                  result->insert(FieldValuesMapPair(std::get<0>(field), value_to<uint64_t>(v)));
                                  break;
                              case kind::double_:
                                  result->insert(FieldValuesMapPair(std::get<0>(field), value_to<double>(v)));
                                  break;
                              case kind::string:
                                  result->insert(FieldValuesMapPair(std::get<0>(field), value_to<std::string>(v)));
                                  break;
                              case kind::array:
                                  result->insert(FieldValuesMapPair(std::get<0>(field), value_to<boost::json::array>(v)));
                                  break;
                              default: break;
                              }
                          }
                      }
                  });
    if (required_fields.size() == result_tmp->size())
    {
        // we have faound all the fields that we need
        result = std::move(result_tmp);
    }
    return result;
}

// parse a json object to get the command
// clang-format off
ConstCommandShrdPtr MapToCommand::parse(const object& obj)
{
    auto logger = ServiceResolver<ILogger>::resolve();
#ifdef __DEBUG__
    logger->logMessage("Received command: " + serialize(obj), LogLevel::DEBUG);
#endif
    ConstCommandShrdPtr result = nullptr;
    switch (getCMDType(obj))
    {
    case CommandType::monitor:
        {
            const std::string       reply_id = check_for_reply_id(obj, logger);
            const std::string       reply_topic = check_reply_topic(obj, logger);
            const SerializationType ser_type = check_for_serialization(obj, SerializationType::Msgpack, logger);
            std::string             event_destination_topic;
            if (auto v = obj.if_contains(KEY_MONITOR_DEST_TOPIC); v != nullptr && v->is_string())
            {
                event_destination_topic = v->as_string();
            }
            else
            {
                // by default uses the reply topic as monitor event
                event_destination_topic = reply_topic;
            }
            if (!event_destination_topic.empty())
            {
                if (auto fields = checkFields(obj, {{KEY_PV_NAME, kind::string}}); fields != nullptr)
                {
                    result = MakeMonitorCommandShrdPtr(
                        ser_type,
                        reply_topic,
                        reply_id,
                        std::any_cast<std::string>(fields->find(KEY_PV_NAME)->second), 
                        event_destination_topic);
                }
                else
                {
                    logger->logMessage("Missing key for the AquireCommand: " + serialize(obj), LogLevel::ERROR);
                }
            }
            else
            {
                logger->logMessage("Impossible to determinate the event destination topic: " + serialize(obj), LogLevel::ERROR);
            }
            break;
        }

    case CommandType::multi_monitor:
        {
            const std::string       reply_id = check_for_reply_id(obj, logger);
            const std::string       reply_topic = check_reply_topic(obj, logger);
            const SerializationType ser_type = check_for_serialization(obj, SerializationType::Msgpack, logger);
            if (auto fields = checkFields(obj, {{KEY_PV_NAME_LIST, kind::array}}); fields != nullptr)
            {
                std::unordered_set<std::string> pv_name_list;
                auto json_array = std::any_cast<boost::json::array>(fields->find(KEY_PV_NAME_LIST)->second);
                // find all stirng in the vector
                for (auto& element : json_array)
                {
                    if (element.kind() != kind::string)
                        continue;
                    pv_name_list.insert(value_to<std::string>(element));
                }
                if (pv_name_list.size())
                {
                    // we can create the command
                    result = MakeMultiMonitorCommandShrdPtr(
                        ser_type,
                        reply_topic,
                        reply_id,
                        pv_name_list
                    );
                }
                else
                {
                    logger->logMessage("The array should not be empty: " + serialize(obj), LogLevel::ERROR);
                }
            }
            else
            {
                logger->logMessage("Missing key for the AquireCommand: " + serialize(obj), LogLevel::ERROR);
            }
            break;
        }

    case CommandType::get:
        {
            if (auto fields = checkFields(obj, {{KEY_PV_NAME, kind::string}, {KEY_REPLY_TOPIC, kind::string}}); fields != nullptr)
            {
                std::string       reply_id = check_for_reply_id(obj, logger);
                SerializationType ser_type = check_for_serialization(obj, SerializationType::Msgpack, logger);
                const std::string reply_topic = check_reply_topic(obj, logger);
                result = MakeGetCommandShrdPtr(
                    ser_type, 
                    reply_topic, 
                    reply_id,
                    std::any_cast<std::string>(fields->find(KEY_PV_NAME)->second));
            }
            else
            {
                logger->logMessage("Missing key for the GetCommand: " + serialize(obj), LogLevel::ERROR);
            }
            break;
        }
    case CommandType::put:
        {
            if (auto fields = checkFields(obj, {{KEY_PV_NAME, kind::string}, {KEY_VALUE, kind::string}}); fields != nullptr)
            {
                std::string       reply_id = check_for_reply_id(obj, logger);
                SerializationType ser_type = check_for_serialization(obj, SerializationType::Msgpack, logger);
                const std::string reply_topic = check_reply_topic(obj, logger);
                result = MakePutCommandShrdPtr(
                    ser_type,
                    reply_topic,
                    reply_id,
                    std::any_cast<std::string>(fields->find(KEY_PV_NAME)->second),
                    std::any_cast<std::string>(fields->find(KEY_VALUE)->second)
                );
            }
            else
            {
                logger->logMessage("Missing key for the PutCommand: " + serialize(obj), LogLevel::ERROR);
            }
            break;
        }
    case CommandType::info:
        {
            if (auto fields = checkFields(obj, {{KEY_PV_NAME, kind::string}, {KEY_REPLY_TOPIC, kind::string}}); fields != nullptr)
            {
                std::string       reply_id = check_for_reply_id(obj, logger);
                SerializationType ser_type = check_for_serialization(obj, SerializationType::Msgpack, logger);
                const std::string reply_topic = check_reply_topic(obj, logger);
                result = MakeInfoCommandShrdPtr(
                    ser_type,
                    reply_topic,
                    reply_id,
                    std::any_cast<std::string>(fields->find(KEY_PV_NAME)->second)
                );
            }
            else
            {
                logger->logMessage("Missing key for the InfoCommand: " + serialize(obj), LogLevel::ERROR);
            }
            break;
        }

    case CommandType::snapshot:
        {
            const SerializationType ser_type = check_for_serialization(obj, SerializationType::Msgpack, logger);
            if (auto fields = checkFields(obj, {{KEY_PV_NAME_LIST, kind::array}, {KEY_REPLY_TOPIC, kind::string}, {KEY_REPLY_ID, kind::string}}); fields != nullptr)
            {
                std::unordered_set<std::string> pv_name_list;

                const std::string        reply_id = check_for_reply_id(obj, logger);
                const std::string        reply_topic = check_reply_topic(obj, logger);
                const int time_window_msec = check_json_field<int32_t>(obj, KEY_TIME_WINDOW_MSEC, logger, "The time window key should be a integer", 1000);
                auto json_array = std::any_cast<boost::json::array>(fields->find(KEY_PV_NAME_LIST)->second);
                // find all stirng in the vector
                for (auto& element : json_array)
                {
                    if (element.kind() != kind::string)
                        continue;
                    pv_name_list.insert(value_to<std::string>(element));
                }

                if (pv_name_list.size())
                {
                    // we can create the command
                    result = MakeGetCommandShrdPtr(
                        ser_type,
                        reply_topic,
                        reply_id,
                        pv_name_list,
                        time_window_msec
                    );
                }
                else
                {
                    logger->logMessage("The array should not be empty: " + serialize(obj), LogLevel::ERROR);
                }
            }
            else
            {
                logger->logMessage("Missing key for the Snapshot: " + serialize(obj), LogLevel::ERROR);
            }
            break;
        }
    case CommandType::repeating_snapshot:
        {
            const SerializationType ser_type = check_for_serialization(obj, SerializationType::Msgpack, logger);
            if (auto fields = checkFields(obj, {{KEY_PV_NAME_LIST, kind::array}, {KEY_REPLY_TOPIC, kind::string}, {KEY_REPLY_ID, kind::string}}); fields != nullptr)
            {
                std::unordered_set<std::string>            pv_name_list;
                std::unordered_set<std::string>     pv_field_filter_list;
                const std::string        reply_id = check_for_reply_id(obj, logger);
                const std::string        reply_topic = check_reply_topic(obj, logger);
                const int repeat_delay_msec = check_json_field<int32_t>(obj, KEY_REPEAT_DELAY_MSEC, logger, "The repeat delay key should be a integer", 0);
                const int time_window_msec = check_json_field<int32_t>(obj, KEY_TIME_WINDOW_MSEC, logger, "The time window key should be a integer", 1000);
                const std::string snapshot_name = check_json_field<std::string>(obj, KEY_SNAPSHOT_NAME, logger, "The snapshot name key should be a string", "");
                const bool triggered = check_json_field<bool>(obj, KEY_TRIGGERED, logger, "The triggered key should be a boolean", false);
                const SnapshotType snapshot_type = snapshot_type_from_string(check_json_field<std::string>(obj, KEY_SNAPSHOT_TYPE, logger, "The snapshot type key should be a string", "normal").c_str());
                const std::int32_t sub_push_delay_msec = check_json_field<int32_t>(obj, KEY_SUB_PUSH_DELAY_MSEC, logger, "The sub push delay key should be a integer", 0);
                auto json_array = std::any_cast<boost::json::array>(fields->find(KEY_PV_NAME_LIST)->second);
                auto json_array_field_filter =  check_json_field<boost::json::array>(obj, KEY_PV_FIELD_FILTER_LIST, logger, "The field filter key should be an array", boost::json::array());
                // find all stirng in the vector
                for (auto& element : json_array)
                {
                    if (element.kind() != kind::string)
                        continue;
                    pv_name_list.insert(value_to<std::string>(element));
                }
                for (auto& element : json_array_field_filter)
                {
                    if (element.kind() != kind::string)
                        continue;
                    pv_field_filter_list.insert(value_to<std::string>(element));
                }
                if (pv_name_list.size())
                {
                    // we can create the command
                    result = MakeRepeatingSnapshotCommandShrdPtr(
                        ser_type,
                        reply_topic,
                        reply_id,
                        snapshot_name,
                        pv_name_list,
                        repeat_delay_msec,
                        time_window_msec,
                        sub_push_delay_msec,
                        triggered,
                        snapshot_type,
                        pv_field_filter_list
                    );
                }
                else
                {
                    logger->logMessage("The array should not be empty: " + serialize(obj), LogLevel::ERROR);
                }
            }
            else
            {
                logger->logMessage("Missing key for the Snapshot: " + serialize(obj), LogLevel::ERROR);
            }
            break;
        }
    case CommandType::repeating_snapshot_stop:
        {
            const SerializationType ser_type = check_for_serialization(obj, SerializationType::Msgpack, logger);
            if (auto fields = checkFields(obj, {{KEY_REPLY_TOPIC, kind::string}, {KEY_REPLY_ID, kind::string}}); fields != nullptr)
            {
                std::vector<std::string> pv_name_list;
                const std::string        reply_id = check_for_reply_id(obj, logger);
                const std::string        reply_topic = check_reply_topic(obj, logger);
                const std::string snapshot_name = check_json_field<std::string>(obj, KEY_SNAPSHOT_NAME, logger, "The snapshot name key should be a string", "");
                result = MakeRepeatingSnapshotStopCommandShrdPtr(
                    ser_type, 
                    reply_topic, 
                    reply_id, 
                    snapshot_name
                );
            }
            else
            {
                logger->logMessage("Missing key for the Snapshot: " + serialize(obj), LogLevel::ERROR);
            }
            break;
        }
    case CommandType::repeating_snapshot_trigger:
        {
            const SerializationType ser_type = check_for_serialization(obj, SerializationType::Msgpack, logger);
            if (auto fields = checkFields(obj, {{KEY_REPLY_TOPIC, kind::string}, {KEY_REPLY_ID, kind::string}}); fields != nullptr)
            {
                std::vector<std::string> pv_name_list;
                const std::string        reply_id = check_for_reply_id(obj, logger);
                const std::string        reply_topic = check_reply_topic(obj, logger);
                const std::string snapshot_name = check_json_field<std::string>(obj, KEY_SNAPSHOT_NAME, logger, "The snapshot name key should be a string", "");
                const std::map<std::string, std::string> trigger_tag = check_json_field_for_map(obj, KEY_TAGS, logger, "The triggered key should be a map");
                result = MakeRepeatingSnapshotTriggerCommandShrdPtr(
                    ser_type,
                    reply_topic,
                    reply_id,
                    snapshot_name,
                    trigger_tag
                );
            }
            else
            {
                logger->logMessage("Missing key for the Snapshot: " + serialize(obj), LogLevel::ERROR);
            }
            break;
        }
    case CommandType::unknown:
        {
            ServiceResolver<ILogger>::resolve()->logMessage("Command not found:" + serialize(obj), LogLevel::ERROR);
            break;
        }
    }
    return result;
}

// clang-format on

void MapToCommand::returnFailCommandParsing(k2eg::service::pubsub::IPublisher& publisher, const boost::json::object& obj)
{
    auto logger = ServiceResolver<ILogger>::resolve();
    // retrieve the minimal information to submite reply to the client
    const std::string       reply_id = check_for_reply_id(obj, logger);
    const std::string       reply_topic = check_reply_topic(obj, logger);
    const SerializationType ser_type = check_for_serialization(obj, SerializationType::Msgpack, logger);
    if (reply_id.empty() || reply_topic.empty())
    {
        logger->logMessage("Impossible to send the error message to the client", LogLevel::ERROR);
        return;
    }
    auto serialized_message = serialize(CommandReply{-1, reply_id}, ser_type);
    if (!serialized_message)
    {
        logger->logMessage("Invalid serialized message", LogLevel::FATAL);
    }
    else
    {
        publisher.pushMessage(MakeReplyPushableMessageUPtr(reply_topic, "comand-parse-error-reply", "comand-parse-error-reply", serialized_message), {{"k2eg-ser-type", serialization_to_string(ser_type)}});
    }
}