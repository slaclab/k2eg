#ifndef K2EG_COMMON_MSGPACKSERIALIZATION_H_
#define K2EG_COMMON_MSGPACKSERIALIZATION_H_
#include <k2eg/common/BaseSerialization.h>
#include <memory>
#include <msgpack.hpp>
#include "k2eg/common/types.h"

namespace k2eg::common {


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