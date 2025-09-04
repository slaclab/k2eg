#ifndef K2EG_CONTROLLER_NODE_WORKER_SNAPSHOT_REPEATINGSNAPSHOTMESSAGES_H_
#define K2EG_CONTROLLER_NODE_WORKER_SNAPSHOT_REPEATINGSNAPSHOTMESSAGES_H_

#include <k2eg/common/BaseSerialization.h>
#include <k2eg/common/types.h>
#include <k2eg/service/epics/EpicsData.h>

#include <cstdint>
#include <memory>
#include <string>

namespace k2eg::controller::node::worker::snapshot {

/*
    @brief Repeating snapshot message header
    @details This message is used to send the header of a repeating snapshot reply.
    The messages that belong to this snapshot are sent sequentially on the same topic.
*/
struct RepeatingSnapshotHeader
{
    const std::int8_t  message_type = 0;
    const std::string  snapshot_name;  // snapshot name
    const std::int64_t timestamp;      // snapshot timestamp
    const std::int64_t iteration_index; // iteration index
};
DEFINE_PTR_TYPES(RepeatingSnapshotHeader)

/*
    @brief Repeating snapshot data message
    @details Carries one PV value for a given snapshot iteration.
*/
struct RepeatingSnapshotData
{
    const std::int8_t  message_type = 1;
    const std::int64_t timestamp;      // snapshot timestamp
    const std::int64_t iteration_index; // iteration index
    k2eg::service::epics_impl::ConstChannelDataShrdPtr pv_data; // data
};
DEFINE_PTR_TYPES(RepeatingSnapshotData)

/*
    @brief Repeating snapshot completion message
    @details Marks the end of a snapshot iteration, optionally with an error.
*/
struct RepeatingSnapshotCompletion
{
    const std::int8_t  message_type = 2;
    const std::int32_t error;           // error code
    const std::string  error_message;   // error message
    const std::string  snapshot_name;   // snapshot name
    const std::int64_t timestamp;       // snapshot timestamp
    const std::int64_t iteration_index; // iteration index
};
DEFINE_PTR_TYPES(RepeatingSnapshotCompletion)

// JSON serialization
inline void serializeJson(const RepeatingSnapshotHeader& event_header, common::JsonMessage& json_message)
{
    json_message.getJsonObject()["message_type"] = event_header.message_type;
    json_message.getJsonObject()["iter_index"] = event_header.iteration_index;
    json_message.getJsonObject()["snapshot_name"] = event_header.snapshot_name;
    json_message.getJsonObject()["timestamp"] = event_header.timestamp;
}

inline void serializeJson(const RepeatingSnapshotData& event_data, common::JsonMessage& json_message)
{
    json_message.getJsonObject()["message_type"] = event_data.message_type;
    json_message.getJsonObject()["iter_index"] = event_data.iteration_index;
    json_message.getJsonObject()["timestamp"] = event_data.timestamp;
    k2eg::service::epics_impl::epics_serializer_factory.resolve(common::SerializationType::JSON)->serialize(*event_data.pv_data, json_message);
}

inline void serializeJson(const RepeatingSnapshotCompletion& event_completion, common::JsonMessage& json_message)
{
    json_message.getJsonObject()["message_type"] = event_completion.message_type;
    json_message.getJsonObject()["error"] = event_completion.error;
    json_message.getJsonObject()["error_message"] = event_completion.error_message;
    json_message.getJsonObject()["iter_index"] = event_completion.iteration_index;
    json_message.getJsonObject()["timestamp"] = event_completion.timestamp;
    json_message.getJsonObject()["snapshot_name"] = event_completion.snapshot_name;
}

// Msgpack serialization
inline void serializeMsgpack(const RepeatingSnapshotHeader& header_event, common::MsgpackMessage& msgpack_message, std::uint8_t = 0)
{
    msgpack::packer<msgpack::sbuffer> packer(msgpack_message.getBuffer());
    packer.pack_map(4);
    packer.pack("message_type");
    packer.pack(header_event.message_type);
    packer.pack("snapshot_name");
    packer.pack(header_event.snapshot_name);
    packer.pack("timestamp");
    packer.pack(header_event.timestamp);
    packer.pack("iter_index");
    packer.pack(header_event.iteration_index);
}

inline void serializeMsgpack(const RepeatingSnapshotData& data_event, common::MsgpackMessage& msgpack_message, std::uint8_t = 0)
{
    msgpack::packer<msgpack::sbuffer> packer(msgpack_message.getBuffer());
    packer.pack_map(4);
    packer.pack("message_type");
    packer.pack(data_event.message_type);
    packer.pack("timestamp");
    packer.pack(data_event.timestamp);
    packer.pack("iter_index");
    packer.pack(data_event.iteration_index);
    k2eg::service::epics_impl::epics_serializer_factory.resolve(common::SerializationType::Msgpack)->serialize(*data_event.pv_data, msgpack_message);
}

inline void serializeMsgpack(const RepeatingSnapshotCompletion& header_completion, common::MsgpackMessage& msgpack_message, std::uint8_t = 0)
{
    msgpack::packer<msgpack::sbuffer> packer(msgpack_message.getBuffer());
    packer.pack_map(header_completion.error_message.empty() ? 5 : 6);
    packer.pack("message_type");
    packer.pack(header_completion.message_type);
    packer.pack("error");
    packer.pack(header_completion.error);
    if (!header_completion.error_message.empty())
    {
        packer.pack("error_message");
        packer.pack(header_completion.error_message);
    }
    packer.pack("iter_index");
    packer.pack(header_completion.iteration_index);
    packer.pack("timestamp");
    packer.pack(header_completion.timestamp);
    packer.pack("snapshot_name");
    packer.pack(header_completion.snapshot_name);
}

// Global helpers to get SerializedMessage wrappers
inline k2eg::common::SerializedMessageShrdPtr serialize(const RepeatingSnapshotHeader& header, k2eg::common::SerializationType type)
{
    switch (type)
    {
    case k2eg::common::SerializationType::JSON:
    {
        auto json_message = std::make_shared<k2eg::common::JsonMessage>();
        serializeJson(header, *json_message);
        return json_message;
    }
    case k2eg::common::SerializationType::Msgpack:
    {
        auto msgpack_message = std::make_shared<k2eg::common::MsgpackMessage>();
        serializeMsgpack(header, *msgpack_message);
        return msgpack_message;
    }
    default: return nullptr;
    }
}

inline k2eg::common::SerializedMessageShrdPtr serialize(const RepeatingSnapshotData& data, k2eg::common::SerializationType type)
{
    switch (type)
    {
    case k2eg::common::SerializationType::JSON:
    {
        auto json_message = std::make_shared<k2eg::common::JsonMessage>();
        serializeJson(data, *json_message);
        return json_message;
    }
    case k2eg::common::SerializationType::Msgpack:
    {
        auto msgpack_message = std::make_shared<k2eg::common::MsgpackMessage>();
        serializeMsgpack(data, *msgpack_message);
        return msgpack_message;
    }
    default: return nullptr;
    }
}

inline k2eg::common::SerializedMessageShrdPtr serialize(const RepeatingSnapshotCompletion& data, k2eg::common::SerializationType type)
{
    switch (type)
    {
    case k2eg::common::SerializationType::JSON:
    {
        auto json_message = std::make_shared<k2eg::common::JsonMessage>();
        serializeJson(data, *json_message);
        return json_message;
    }
    case k2eg::common::SerializationType::Msgpack:
    {
        auto msgpack_message = std::make_shared<k2eg::common::MsgpackMessage>();
        serializeMsgpack(data, *msgpack_message);
        return msgpack_message;
    }
    default: return nullptr;
    }
}

} // namespace k2eg::controller::node::worker::snapshot

#endif // K2EG_CONTROLLER_NODE_WORKER_SNAPSHOT_REPEATINGSNAPSHOTMESSAGES_H_

