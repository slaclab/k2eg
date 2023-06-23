#ifndef K2EG_COMMON_JSONSERIALIZATION_H_
#define K2EG_COMMON_JSONSERIALIZATION_H_

#include <k2eg/common/serialization.h>

#include <boost/json.hpp>
#include <sstream>
#include <string>

#include "k2eg/common/BaseSerialization.h"
#include "k2eg/common/types.h"
namespace k2eg::common {

class JsonMessage;
class JsonData : public Data {
  std::string json_buf;
 public:
   JsonData(const boost::json::object &json_object):Data() {
    std::stringstream ss;
    ss << json_object;
    json_buf = ss.str();
  }
  virtual ~JsonData() = default;

  const size_t
  size() const {
    return json_buf.size();
  }

  const char *
  data() const {
    return json_buf.c_str();
  }
};
DEFINE_PTR_TYPES(JsonData)
/**
 */
class JsonMessage : public k2eg::common::SerializedMessage {
  boost::json::object json_object;
 public:
  JsonMessage()  = default;
  ~JsonMessage() = default;
  boost::json::object& getJsonObject() {
    return json_object;
  }
  ConstDataUPtr
  data() const {
    return MakeJsonDataUPtr(json_object);
  }
};
DEFINE_PTR_TYPES(JsonMessage)
}  // namespace k2eg::common

#endif  // K2EG_COMMON_JSONSERIALIZATION_H_