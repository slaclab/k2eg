#include <gtest/gtest.h>
#include <k2eg/controller/node/worker/CommandWorker.h>

#include <memory>
#include <vector>

#include "k2eg/common/types.h"
#include "msgpack/v3/object_fwd_decl.hpp"

using namespace k2eg::common;
using namespace k2eg::controller::node::worker;

TEST(NodeControllerReply, JsonReplySerialization) {
  boost::json::value       jsonReply     = {{"string", "string"},
                                            {"number", 30},
                                            {"bool", true},
                                            {"object", {{"key1", "value1"}, {"key2", "value2"}, {"key3", "value3"}}},
                                            {"array-object",
                                             {{
                                        "key-a",
                                        "v-a",
                                    },
                                              {
                                        "key-b",
                                        "vb",
                                    }}}};
  auto                     rs            = ReplySerializer(std::make_shared<boost::json::value>(jsonReply));
  auto                     jsonRepResult = rs.serialize(SerializationType::JSON);
  boost::json::string_view sv(jsonRepResult->data(), jsonRepResult->size());
  boost::json::value       jv = boost::json::parse(sv);
  ASSERT_EQ(jsonReply == jv, true);
}

typedef std::map<std::string, msgpack::object> MapTest;
typedef std::vector<msgpack::object>           VectorTest;

TEST(NodeControllerReply, MsgpackReplySerializationMap) {
  boost::json::value     jsonReply = {{"key1", "value1"}, {"key2", "value2"}, {"key3", "value3"}};
  auto                   rs        = ReplySerializer(std::make_shared<boost::json::value>(jsonReply));
  auto                   repRes    = rs.serialize(SerializationType::Msgpack);
  msgpack::object_handle obj;
  EXPECT_NO_THROW(msgpack::unpack(obj, repRes->data(), repRes->size()););
  EXPECT_EQ(msgpack::type::MAP, obj->type);
  auto om = obj.get().as<MapTest>();
  EXPECT_EQ(om.contains("key1"), true);
  EXPECT_EQ(om.contains("key2"), true);
  EXPECT_EQ(om.contains("key3"), true);
}

TEST(NodeControllerReply, MsgpackCompactReplySerializationMap) {
  boost::json::value     jsonReply = {{"key1", "value1"}, {"key2", "value2"}, {"key3", "value3"}};
  auto                   rs        = ReplySerializer(std::make_shared<boost::json::value>(jsonReply));
  auto                   repRes    = rs.serialize(SerializationType::MsgpackCompact);
  msgpack::object_handle obj;
  EXPECT_NO_THROW(msgpack::unpack(obj, repRes->data(), repRes->size()););
  EXPECT_EQ(msgpack::type::ARRAY, obj->type);
  auto oarr = obj.get().as<VectorTest>();
  EXPECT_STREQ(oarr[0].as<std::string>().c_str(), "value1");
  EXPECT_STREQ(oarr[1].as<std::string>().c_str(), "value2");
  EXPECT_STREQ(oarr[2].as<std::string>().c_str(), "value3");
}

TEST(NodeControllerReply, MsgpackReplySerializationArray) {
  boost::json::value     jsonReply = {{"key1", {"v1", "v2", "v3"}}};
  auto                   rs        = ReplySerializer(std::make_shared<boost::json::value>(jsonReply));
  auto                   repRes    = rs.serialize(SerializationType::Msgpack);
  msgpack::object_handle obj;
  EXPECT_NO_THROW(msgpack::unpack(obj, repRes->data(), repRes->size()););
  EXPECT_EQ(msgpack::type::MAP, obj->type);
  auto om = obj.get().as<MapTest>();
  EXPECT_EQ(om.contains("key1"), true);
  EXPECT_EQ(om["key1"].type, msgpack::type::ARRAY);
  auto oarr = om["key1"].as<VectorTest>();
  EXPECT_STREQ(oarr[0].as<std::string>().c_str(), "v1");
  EXPECT_STREQ(oarr[1].as<std::string>().c_str(), "v2");
  EXPECT_STREQ(oarr[2].as<std::string>().c_str(), "v3");
}

TEST(NodeControllerReply, MsgpackCompactReplySerializationARRAY) {
  boost::json::value     jsonReply = {{"key1", {"v1", "v2", "v3"}}};
  auto                   rs        = ReplySerializer(std::make_shared<boost::json::value>(jsonReply));
  auto                   repRes    = rs.serialize(SerializationType::MsgpackCompact);
  msgpack::object_handle obj;
  EXPECT_NO_THROW(msgpack::unpack(obj, repRes->data(), repRes->size()););
  EXPECT_EQ(msgpack::type::ARRAY, obj->type);
  auto oarr = obj.get().as<VectorTest>();
  EXPECT_EQ(msgpack::type::ARRAY, oarr[0].type);
}