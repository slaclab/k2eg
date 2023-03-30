#include <k2eg/service/epics/JsonSerializer.h>

#include <pv/bitSet.h>
#include <pv/json.h>
#include <sstream>

using namespace k2eg::service::epics_impl;

REGISTER_SERIALIZER(SerializationType::JSON, JsonSerializer)

ConstSerializedMessageUPtr JsonSerializer::serialize(const ChannelData& message) {
    return ConstSerializedMessageUPtr();
}


std::string to_json(const ChannelData& channel_data, const std::set<std::string>& field) {
    std::stringstream ss;
    ss << "{\"" << channel_data.channel_name << "\":{";
    epics::pvData::BitSet bs_mask;
    epics::pvData::JSONPrintOptions opt;
    opt.ignoreUnprintable = true;
    opt.indent = 0;
    opt.multiLine = false;
    for (size_t idx = 1, N = channel_data.data->getStructure()->getNumberFields(); idx < N; idx++) {
        bs_mask.set(idx);
    }
    epics::pvData::printJSON(ss, *channel_data.data, bs_mask, opt);
    ss << "}";
    return ss.str();
}