
#ifndef k2eg_CONTROLLER_COMMAND_CMDOPCODE_H_
#define k2eg_CONTROLLER_COMMAND_CMDOPCODE_H_

#include <k2eg/common/types.h>
#include <k2eg/controller/command/cmd/Command.h>
#include <k2eg/controller/command/cmd/InfoCommand.h>
#include <k2eg/controller/command/cmd/GetCommand.h>
#include <k2eg/controller/command/cmd/PutCommand.h>
#include <k2eg/controller/command/cmd/MonitorCommand.h>

#include <any>
#include <boost/json.hpp>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace k2eg::controller::command {
#define JSON_VALUE_TO(t, v) boost::json::value_to<t>(v)

DEFINE_MAP_FOR_TYPE(std::string, std::any, FieldValuesMap)
typedef std::unique_ptr<FieldValuesMap> FieldValuesMapUPtr;

#define BOOST_JSON_TO_STRIN(t, x) \
boost::json::serialize(boost::json::value_from( \
            *static_pointer_cast<const t>(c) \
        ));


static const std::string to_json_string(cmd::ConstCommandShrdPtr c) {
    switch(c->type) {
    case cmd::CommandType::get:
        return BOOST_JSON_TO_STRIN(cmd::GetCommand, c);
    case cmd::CommandType::info:
        return BOOST_JSON_TO_STRIN(cmd::InfoCommand, c);
    case cmd::CommandType::monitor:
        return BOOST_JSON_TO_STRIN(cmd::MonitorCommand, c);
    case cmd::CommandType::put:
        return BOOST_JSON_TO_STRIN(cmd::PutCommand, c);
    case cmd::CommandType::unknown:
        return "Unknown";
    }
    return "Unknown";
}


/**
 * class that help to map the json structure to a command
 */
class MapToCommand {
    static k2eg::controller::command::cmd::MessageSerType getSerializationType(const  boost::json::value& v);
    /**
     * Extract the command type
     */
    static cmd::CommandType getCMDType(const boost::json::object& ob);
    /**
     * Verify the presence of all the filed within the json object
     */
    static FieldValuesMapUPtr checkFields(const boost::json::object& obj,
                                          const std::vector<std::tuple<std::string, boost::json::kind>>& fields);

public:
    static cmd::ConstCommandShrdPtr parse(const boost::json::object& obj);
};

} // namespace k2eg::controller::command

#endif // k2eg_CONTROLLER_COMMAND_CMDOPCODE_H_
