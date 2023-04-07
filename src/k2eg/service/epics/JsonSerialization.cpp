#include <k2eg/service/epics/JsonSerialization.h>

#include <pv/bitSet.h>
#include <pv/json.h>
#include <sstream>
using namespace k2eg::service::epics_impl;

#pragma region JsonMessage
JsonMessage::JsonMessage(std::string& json_object)
    : json_object(std::move(json_object)) {}

const size_t JsonMessage::size() const {
    return json_object.size();
}
const char* JsonMessage::data() const {
    return json_object.c_str();
}
#pragma endregion JsonMessage


#pragma region JsonSerializer
REGISTER_SERIALIZER(SerializationType::JSON, JsonSerializer)
ConstSerializedMessageUPtr JsonSerializer::serialize(const ChannelData& message) {
    std::stringstream ss;
    ss << "{\"" << message.channel_name << "\":{";
    epics::pvData::BitSet bs_mask;
    epics::pvData::JSONPrintOptions opt;
    opt.ignoreUnprintable = true;
    opt.indent = 0;
    opt.multiLine = false;
    for (size_t idx = 1, N = message.data->getStructure()->getNumberFields(); idx < N; idx++) {
        bs_mask.set(idx);
    }
    epics::pvData::printJSON(ss, *message.data, bs_mask, opt);
    ss << "}";
    return MakeJsonMessageUPtr(std::move(ss.str()));
}
#pragma endregion JsonSerializer