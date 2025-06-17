#ifndef K2EG_COMMON_MSGPACKSERIALIZATION_H_
#define K2EG_COMMON_MSGPACKSERIALIZATION_H_
#include <k2eg/common/BaseSerialization.h>
#include <k2eg/common/types.h>
#include <memory>
#include <msgpack.hpp>
#include <vector>

namespace k2eg::common {

class MsgpackObject
{

public:
    MsgpackObject() = default;
    virtual ~MsgpackObject() = default;
    virtual msgpack::object            get() const = 0;
    virtual std::vector<unsigned char> toBuffer() const = 0;
};

class MsgpackObjectWithZone : public MsgpackObject
{
    msgpack::object_handle handle;
    msgpack::unpacked      msg;

public:
    // Existing constructor from value
    template <class T>
    MsgpackObjectWithZone(const T& val)
    {
        msgpack::sbuffer buffer;
        msgpack::pack(buffer, val);
        handle = msgpack::unpack(buffer.data(), buffer.size());
    }

    // New constructor from handle
    MsgpackObjectWithZone(msgpack::object_handle&& h) : handle(std::move(h)) {}

    virtual ~MsgpackObjectWithZone() = default;

    msgpack::object get() const
    {
        return handle.get();
    }

    std::vector<unsigned char> toBuffer() const
    {
        msgpack::sbuffer sbuf;
        msgpack::pack(sbuf, handle.get());
        return std::vector<unsigned char>(sbuf.data(), sbuf.data() + sbuf.size());
    }
};

class MsgpackObjectFromBuffer : public MsgpackObject
{
    const std::vector<unsigned char> buff;
    msgpack::object_handle           oh;

public:
    // New constructor from handle
    MsgpackObjectFromBuffer(const std::vector<unsigned char>&& buff) : buff(std::move(buff))
    {
        msgpack::unpack(oh, (const char*)this->buff.data(), buff.size());
    }

    virtual ~MsgpackObjectFromBuffer() = default;

    msgpack::object get() const
    {
        return oh.get();
    }

    std::vector<unsigned char> toBuffer() const
    {
        return buff; // Return the original buffer
    }
};

inline std::unique_ptr<MsgpackObject> unpack_msgpack_object(std::vector<unsigned char>&& data)
{
    msgpack::object_handle oh = msgpack::unpack(reinterpret_cast<const char*>(data.data()), data.size());
    return std::make_unique<MsgpackObjectFromBuffer>(std::move(data));
}

class MsgpackMessage;

class MsgpackData : public Data
{
    std::shared_ptr<msgpack::sbuffer> buffer;

public:
    MsgpackData(std::shared_ptr<msgpack::sbuffer> buffer) : Data(), buffer(buffer) {}

    virtual ~MsgpackData() = default;

    const size_t size() const
    {
        return buffer->size();
    }

    const char* data() const
    {
        return buffer->data();
    }
};

DEFINE_PTR_TYPES(MsgpackData);

/**
 */
class MsgpackMessage : public k2eg::common::SerializedMessage
{
    std::shared_ptr<msgpack::sbuffer> buffer;

public:
    MsgpackMessage() : buffer(std::make_shared<msgpack::sbuffer>()) {}

    ~MsgpackMessage() = default;

    msgpack::sbuffer& getBuffer()
    {
        return *buffer;
    }

    ConstDataUPtr data() const
    {
        return MakeMsgpackDataUPtr(buffer);
    }
};
DEFINE_PTR_TYPES(MsgpackMessage)
} // namespace k2eg::common

#endif // K2EG_COMMON_MSGPACKSERIALIZATION_H_namespace name = ;