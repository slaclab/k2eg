#include <gtest/gtest.h>
#include <k2eg/common/serialization.h>
#include <k2eg/controller/node/worker/CommandWorker.h>
#include <k2eg/controller/node/worker/GetCommandWorker.h>
#include <k2eg/controller/node/worker/PutCommandWorker.h>
#include <k2eg/controller/node/worker/MonitorCommandWorker.h>
#include <k2eg/common/BaseSerialization.h>

#include <msgpack/v3/object_fwd_decl.hpp>
#include <map>

#include "../epics/epics.h"

using namespace k2eg::common;
using namespace k2eg::controller::node::worker;
using namespace k2eg::controller::node::worker::monitor;
using namespace k2eg::service::epics_impl;

msgpack::unpacked
getMsgPackObject(SerializedMessage& message) {
  msgpack::unpacked msg_upacked;
  auto data = message.data();
  msgpack::unpack(msg_upacked, data->data(), data->size());
  return msg_upacked;
}

TEST(NodeControllerReplyMessages, BaseCommandReplyJson) {
  CommandReply cr            = {0, "rep_id"};
  auto         serialization = serialize(cr, SerializationType::JSON);
  EXPECT_NE(serialization, nullptr);
};

TEST(NodeControllerReplyMessages, GetCommandReplyJson) {
  INIT_PVA_PROVIDER()
  EpicsChannelUPtr      pc;
  ConstDataUPtr         ser_value;
  ConstGetOperationUPtr get_op;
  EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_pva_provider, "variable:sum"););
  EXPECT_NO_THROW(get_op = pc->get(););
  WHILE_OP(get_op, false);
  GetCommandReply cr            = {0, "rep_id", get_op->getChannelData()};
  auto            serialization = serialize(cr, SerializationType::JSON);
  EXPECT_NE(serialization, nullptr);
};

TEST(NodeControllerReplyMessages, GetFaultyCommandReplyJson) {
  GetFaultyCommandReply cr            = {-1, "rep_id", "message"};
  auto            serialization = serialize(cr, SerializationType::JSON);
  EXPECT_NE(serialization, nullptr);
};

TEST(NodeControllerReplyMessages, GetCommandReplyMsgpack) {
  INIT_PVA_PROVIDER()
  EpicsChannelUPtr      pc;
  ConstDataUPtr         ser_value;
  ConstGetOperationUPtr get_op;
  EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_pva_provider, "variable:sum"););
  EXPECT_NO_THROW(get_op = pc->get(););
  WHILE_OP(get_op, false);
  GetCommandReply cr            = {0, "rep_id", get_op->getChannelData()};
  auto            serialization = serialize(cr, SerializationType::Msgpack);
  EXPECT_NE(serialization, nullptr);
  auto msg = getMsgPackObject(*serialization);
  auto msgpack_object = msg.get();
};

TEST(NodeControllerReplyMessages, GetFaultyCommandReplyMsgpack) {
  GetFaultyCommandReply cr            = {-1, "rep_id", "message"};
  auto            serialization = serialize(cr, SerializationType::Msgpack);
  EXPECT_NE(serialization, nullptr);
  auto msg = getMsgPackObject(*serialization);
  auto msgpack_object = msg.get();
  auto deserialized_map = msgpack_object.as<std::map<std::string, msgpack::object>>();
  EXPECT_EQ(deserialized_map.contains("error"), true);
  EXPECT_EQ(deserialized_map["error"].type, msgpack::type::NEGATIVE_INTEGER);
  EXPECT_EQ(deserialized_map.contains("message"), true);
  EXPECT_EQ(deserialized_map["message"].type, msgpack::type::STR);
  EXPECT_EQ(deserialized_map.contains(KEY_REPLY_ID), true);
  EXPECT_EQ(deserialized_map[KEY_REPLY_ID].type, msgpack::type::STR);
};

TEST(NodeControllerReplyMessages, GetCommandReplyMsgpackCompact) {
  INIT_PVA_PROVIDER()
  EpicsChannelUPtr      pc;
  ConstDataUPtr         ser_value;
  ConstGetOperationUPtr get_op;
  EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_pva_provider, "variable:sum"););
  EXPECT_NO_THROW(get_op = pc->get(););
  WHILE_OP(get_op, false);
  GetCommandReply cr            = {0, "rep_id", get_op->getChannelData()};
  auto            serialization = serialize(cr, SerializationType::MsgpackCompact);
  EXPECT_NE(serialization, nullptr);
  auto msg = getMsgPackObject(*serialization);
  auto msgpack_object = msg.get();
};

TEST(NodeControllerReplyMessages, GetFaultyCommandReplyMsgpackCompact) {
  GetFaultyCommandReply cr            = {-1, "rep_id", "message"};
  auto            serialization = serialize(cr, SerializationType::MsgpackCompact);
  EXPECT_NE(serialization, nullptr);
  auto msg = getMsgPackObject(*serialization);
  auto msgpack_object = msg.get();
  auto deserialized_map = msgpack_object.as<std::map<std::string, msgpack::object>>();
  EXPECT_EQ(deserialized_map.contains("error"), true);
  EXPECT_EQ(deserialized_map["error"].type, msgpack::type::NEGATIVE_INTEGER);
  EXPECT_EQ(deserialized_map.contains("message"), true);
  EXPECT_EQ(deserialized_map["message"].type, msgpack::type::STR);
  EXPECT_EQ(deserialized_map.contains(KEY_REPLY_ID), true);
  EXPECT_EQ(deserialized_map[KEY_REPLY_ID].type, msgpack::type::STR);

};

TEST(NodeControllerReplyMessages, PutCommandReplyJson) {
  PutCommandReply cr            = {0, "rep_id"};
  auto            serialization = serialize(cr, SerializationType::JSON);
  EXPECT_NE(serialization, nullptr);
};

TEST(NodeControllerReplyMessages, PutCommandReplyMsgpack) {;
  PutCommandReply cr            = {0, "rep_id"};
  auto            serialization = serialize(cr, SerializationType::Msgpack);
  EXPECT_NE(serialization, nullptr);
  auto msg = getMsgPackObject(*serialization);
  auto msgpack_object = msg.get();
};

TEST(NodeControllerReplyMessages, PutCommandReplyMsgpackCompact) {
  PutCommandReply cr            = {0, "rep_id"};
  auto            serialization = serialize(cr, SerializationType::MsgpackCompact);
  EXPECT_NE(serialization, nullptr);
  auto msg = getMsgPackObject(*serialization);
  auto msgpack_object = msg.get();
};

TEST(NodeControllerReplyMessages, PutCommandReplyWithMessage) {
  PutCommandReply cr            = {0, "rep_id", "error-message"};
  auto            serialization = serialize(cr, SerializationType::MsgpackCompact);
  EXPECT_NE(serialization, nullptr);
  auto msg = getMsgPackObject(*serialization);
  auto msgpack_object = msg.get();
};


TEST(NodeControllerReplyMessages, MonitorCommandActivateReply) {
  typedef std::map<std::string, msgpack::object> Map;
  MonitorCommandReply cr            = {0, "rep_id", "Activated"};
  auto            serialization = serialize(cr, SerializationType::Msgpack);
  EXPECT_NE(serialization, nullptr);
  auto msg = getMsgPackObject(*serialization);
  auto msgpack_object = msg.get();
  auto message_map = msgpack_object.as<Map>();
  EXPECT_EQ(message_map.contains("message"), true);
  EXPECT_STREQ(message_map["message"].as<std::string>().c_str(), "Activated");
};