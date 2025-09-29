#ifndef k2eg_CONTROLLER_NODE_WORKER_COMMANDWORKER_H_
#define k2eg_CONTROLLER_NODE_WORKER_COMMANDWORKER_H_

#include <k2eg/common/BS_thread_pool.hpp>
#include <k2eg/common/JsonSerialization.h>
#include <k2eg/common/MsgpackSerialization.h>
#include <k2eg/common/serialization.h>
#include <k2eg/common/types.h>

#include <k2eg/service/pubsub/IPublisher.h>

#include <k2eg/controller/command/CMDCommand.h>
#include <k2eg/controller/command/cmd/Command.h>

#include <msgpack/v3/pack_decl.hpp>

#include <chrono>
#include <cstdint>
#include <memory>

namespace k2eg::controller::node::worker {

/**
 * @brief Base reply payload for command handlers.
 *
 * Some commands produce a reply; this struct carries the common fields
 * included in all replies produced by node workers.
 */
struct CommandReply
{
    /** @brief Error code of the executed command (0 = success). */
    const std::int8_t error;
    /** @brief Correlation id from the originating command. */
    const std::string reply_id;
};

static void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, CommandReply const& reply)
{
    jv = {{"error", reply.error}, {KEY_REPLY_ID, reply.reply_id}};
}

/**
 * @brief Serialize base reply fields to JSON.
 * @param reply reply object to serialize
 * @param json_message destination JSON message
 */
inline void serializeJson(const CommandReply& reply, common::JsonMessage& json_message)
{
    json_message.getJsonObject()["error"] = reply.error;
    json_message.getJsonObject()[KEY_REPLY_ID] = reply.reply_id;
}

/**
 * @brief Serialize base reply fields to Msgpack.
 * @param reply reply object to serialize
 * @param msgpack_message destination msgpack message
 * @param map_size additional map entries contributed by caller
 */
inline void serializeMsgpack(const CommandReply& reply, common::MsgpackMessage& msgpack_message, std::uint8_t map_size = 0)
{
    msgpack::packer<msgpack::sbuffer> packer(msgpack_message.getBuffer());
    packer.pack_map(map_size + 2);
    packer.pack("error");
    packer.pack(reply.error);
    packer.pack(KEY_REPLY_ID);
    packer.pack(reply.reply_id);
}

/**
 * @brief Serialize base reply fields to compact Msgpack.
 * @param reply reply object to serialize
 * @param msgpack_message destination msgpack message
 * @param map_size additional map entries contributed by caller
 */
inline void serializeMsgpackCompact(const CommandReply& reply, common::MsgpackMessage& msgpack_message, std::uint8_t map_size = 0)
{
    msgpack::packer<msgpack::sbuffer> packer(msgpack_message.getBuffer());
    packer.pack_map(map_size + 2);
    packer.pack("error");
    packer.pack(reply.error);
    packer.pack(KEY_REPLY_ID);
    packer.pack(reply.reply_id);
}

template <class ReplyObjectClass>
inline common::SerializedMessageShrdPtr serialize(const ReplyObjectClass& reply, k2eg::common::SerializationType ser_type)
{
    common::SerializedMessageShrdPtr result;
    switch (ser_type)
    {
    case common::SerializationType::Unknown: return common::SerializedMessageShrdPtr();
    case common::SerializationType::JSON:
        {
            result = std::make_shared<common::JsonMessage>();
            serializeJson(reply, *static_pointer_cast<common::JsonMessage>(result));
            break;
        };
    case common::SerializationType::Msgpack:
        {
            result = std::make_shared<common::MsgpackMessage>();
            serializeMsgpack(reply, *static_pointer_cast<common::MsgpackMessage>(result));
            break;
        };
    case common::SerializationType::MsgpackCompact:
        {
            result = std::make_shared<common::MsgpackMessage>();
            serializeMsgpackCompact(reply, *static_pointer_cast<common::MsgpackMessage>(result));
            break;
        };
    }
    return result;
}

/**
 * @brief Pushable message wrapper for replies.
 *
 * Adapts a serialized reply into the publisher interface with queue,
 * type, and distribution key metadata.
 */
class ReplyPushableMessage : public k2eg::service::pubsub::PublishMessage
{
    const std::string                           queue;
    const std::string                           type;
    const std::string                           distribution_key;
    k2eg::common::ConstSerializedMessageShrdPtr message;
    k2eg::common::ConstDataUPtr                 message_data;

public:
    ReplyPushableMessage(const std::string& queue, const std::string& type, const std::string& distribution_key, k2eg::common::ConstSerializedMessageShrdPtr message);
    virtual ~ReplyPushableMessage();
    /** @brief Raw pointer to payload buffer. */
    char*              getBufferPtr();
    /** @brief Size of payload buffer in bytes. */
    const size_t       getBufferSize();
    /** @brief Destination queue/topic. */
    const std::string& getQueue();
    /** @brief Distribution key used for partitioning. */
    const std::string& getDistributionKey();
    /** @brief Request/reply type identifier. */
    const std::string& getReqType();
};
DEFINE_PTR_TYPES(ReplyPushableMessage)

/**
 * @brief Base class for long-running asynchronous worker operations.
 */
class WorkerAsyncOperation
{
    const std::chrono::milliseconds       timeout_ms;
    std::chrono::steady_clock::time_point begin;

public:
    WorkerAsyncOperation(std::chrono::milliseconds timeout_ms)
        : timeout_ms(timeout_ms), begin(std::chrono::steady_clock::now())
    {
    }

    /**
     * @brief Check and advance the timeout window.
     * @param now Current time (default now).
     * @return true if the operation exceeded its timeout; false otherwise.
     */
    virtual bool isTimeout(const std::chrono::steady_clock::time_point& now = std::chrono::steady_clock::now())
    {
        if ((now - begin) > timeout_ms)
        {
            begin = now; // renew begin after timeout
            return true;
        }
        return false;
    }
};

/**
 * @brief Abstract worker that processes commands.
 */
class CommandWorker
{
public:
    CommandWorker() = default;
    CommandWorker(const CommandWorker&) = delete;
    CommandWorker& operator=(const CommandWorker&) = delete;
    ~CommandWorker() = default;
    /**
     * @brief Process a command asynchronously using the provided pool.
     */
    virtual void processCommand(std::shared_ptr<BS::light_thread_pool> command_pool, k2eg::controller::command::cmd::ConstCommandShrdPtr command) = 0;
    /** @brief Whether this worker is ready to process commands. */
    virtual bool        isReady();
    /** @brief Number of tasks currently running for this worker. */
    virtual std::size_t getTaskRunning() const;
};
DEFINE_PTR_TYPES(CommandWorker)
} // namespace k2eg::controller::node::worker

#endif // k2eg_CONTROLLER_NODE_WORKER_COMMANDWORKER_H_
