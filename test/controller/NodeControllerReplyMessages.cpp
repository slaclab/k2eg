#include <gtest/gtest.h>
#include <k2eg/common/serialization.h>
#include <k2eg/controller/node/worker/CommandWorker.h>
#include <k2eg/controller/node/worker/GetCommandWorker.h>
#include <k2eg/controller/node/worker/PutCommandWorker.h>

#include "../epics/epics.h"
#include "k2eg/common/BaseSerialization.h"

using namespace k2eg::common;
using namespace k2eg::controller::node::worker;
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
  std::cout << std::string(serialization->data()->data(), serialization->data()->size()) << std::endl;
};

TEST(NodeControllerReplyMessages, GetCommandReplyJson) {
  INIT_PVA_PROVIDER()
  EpicsChannelUPtr      pc;
  ConstDataUPtr         ser_value;
  ConstGetOperationUPtr get_op;
  EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_pva_provider, "variable:sum"););
  EXPECT_NO_THROW(get_op = pc->get(););
  WHILE(get_op->isDone(), false);
  GetCommandReply cr            = {0, "rep_id", get_op->getChannelData()};
  auto            serialization = serialize(cr, SerializationType::JSON);
  EXPECT_NE(serialization, nullptr);
  std::cout << std::string(serialization->data()->data(), serialization->data()->size()) << std::endl;
};

TEST(NodeControllerReplyMessages, GetCommandReplyMsgpack) {
  INIT_PVA_PROVIDER()
  EpicsChannelUPtr      pc;
  ConstDataUPtr         ser_value;
  ConstGetOperationUPtr get_op;
  EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_pva_provider, "variable:sum"););
  EXPECT_NO_THROW(get_op = pc->get(););
  WHILE(get_op->isDone(), false);
  GetCommandReply cr            = {0, "rep_id", get_op->getChannelData()};
  auto            serialization = serialize(cr, SerializationType::Msgpack);
  EXPECT_NE(serialization, nullptr);
  auto msg = getMsgPackObject(*serialization);
  auto msgpack_object = msg.get();
  std::cout << msgpack_object << std::endl;
};

TEST(NodeControllerReplyMessages, GetCommandReplyMsgpackCompact) {
  INIT_PVA_PROVIDER()
  EpicsChannelUPtr      pc;
  ConstDataUPtr         ser_value;
  ConstGetOperationUPtr get_op;
  EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_pva_provider, "variable:sum"););
  EXPECT_NO_THROW(get_op = pc->get(););
  WHILE(get_op->isDone(), false);
  GetCommandReply cr            = {0, "rep_id", get_op->getChannelData()};
  auto            serialization = serialize(cr, SerializationType::MsgpackCompact);
  EXPECT_NE(serialization, nullptr);
  auto msg = getMsgPackObject(*serialization);
  auto msgpack_object = msg.get();
  std::cout << msgpack_object << std::endl;
};