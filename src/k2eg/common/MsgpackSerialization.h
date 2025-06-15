#ifndef K2EG_COMMON_MSGPACKSERIALIZATION_H_
#define K2EG_COMMON_MSGPACKSERIALIZATION_H_
#include <k2eg/common/BaseSerialization.h>
#include <memory>
#include <msgpack.hpp>
#include <k2eg/common/types.h>

namespace k2eg::common {

struct MsgpackObjectWithZone {
    std::shared_ptr<msgpack::zone> zone;
    std::shared_ptr<msgpack::object> object;
};

inline std::unique_ptr<MsgpackObjectWithZone> unpack_msgpack_object(const std::vector<unsigned char>& data) {
    msgpack::object_handle oh = msgpack::unpack(reinterpret_cast<const char*>(data.data()), data.size());
    auto zone = std::make_unique<msgpack::zone>();
    auto object = std::make_unique<msgpack::object>(oh.get(), *zone);
    return std::make_unique<MsgpackObjectWithZone>(MsgpackObjectWithZone{std::move(zone), std::move(object)});
}

class MsgpackMessage;
class MsgpackData : public Data {
  std::shared_ptr<msgpack::sbuffer> buffer;
 public:
  MsgpackData(std::shared_ptr<msgpack::sbuffer> buffer)
  :Data()
  , buffer(buffer) {}
  virtual ~MsgpackData() = default;

  const size_t
  size() const {
    return buffer->size();
  }

  const char *
  data() const {
    return buffer->data();
  }
};
DEFINE_PTR_TYPES(MsgpackData);
/**
*/
class MsgpackMessage : public k2eg::common::SerializedMessage {
    std::shared_ptr<msgpack::sbuffer> buffer;
    public:
    MsgpackMessage():buffer(std::make_shared<msgpack::sbuffer>()){}
    ~MsgpackMessage()= default;
    msgpack::sbuffer& getBuffer(){
        return *buffer;
    }
    ConstDataUPtr 
    data() const{
        return MakeMsgpackDataUPtr(buffer);
    }
};
DEFINE_PTR_TYPES(MsgpackMessage)
}

#endif // K2EG_COMMON_MSGPACKSERIALIZATION_H_namespace name = ;