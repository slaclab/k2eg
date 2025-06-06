#include <gtest/gtest.h>
#include <k2eg/service/epics/EpicsChannel.h>
#include <k2eg/service/epics/EpicsServiceManager.h>

#include <boost/json.hpp>
#include <msgpack.hpp>

#include "epics.h"
#include "k2eg/common/BaseSerialization.h"
#include "k2eg/service/epics/EpicsMonitorOperation.h"
#include "msgpack/v3/object_fwd_decl.hpp"

using namespace k2eg::common;
using namespace k2eg::service::epics_impl;

typedef std::vector<msgpack::object> MsgpackObjectVector;

TEST(EpicsChannel, SerializationJSON) {
  INIT_PVA_PROVIDER()
  EpicsChannelUPtr      pc;
  ConstDataUPtr         ser_value;
  ConstGetOperationUPtr get_op;
  EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_pva_provider, "variable:sum"););
  // EXPECT_NO_THROW(pc->connect());
  EXPECT_NO_THROW(get_op = pc->get(););
  WHILE_OP(get_op, false);
  EXPECT_NO_THROW(ser_value = serialize(*get_op->getChannelData(), SerializationType::JSON)->data(););
  EXPECT_NE(ser_value, nullptr);
  EXPECT_NE(ser_value->data(), nullptr);
  EXPECT_NE(ser_value->size(), 0);
  std::string string_value(ser_value->data(), ser_value->size());
  // {"variable:sum":{"value":7E0,"alarm":{"severity":0,"status":0,"message":"NO_ALARM"},"timeStamp":{"secondsPastEpoch":1681018040,"nanoseconds":899757791,"userTag":0},"display":{"limitLow":0E0,"limitHigh":0E0,"description":"","units":"","precision":0,"form":{"index":0}},"control":{"limitLow":0E0,"limitHigh":0E0,"minStep":0E0},"valueAlarm":{"active":0,"lowAlarmLimit":"NaN","lowWarningLimit":"NaN","highWarningLimit":"NaN","highAlarmLimit":"NaN","lowAlarmSeverity":0,"lowWarningSeverity":0,"highWarningSeverity":0,"highAlarmSeverity":0,"hysteresis":0}}}
  boost::system::error_code ec;
  boost::json::value      jv;
  EXPECT_NO_THROW(jv = boost::json::parse(string_value, ec););
  EXPECT_EQ(ec.value(), false);
  EXPECT_EQ(jv.as_object().contains("variable:sum"), true);
  auto sub_obj = jv.as_object().at("variable:sum").as_object();
  EXPECT_EQ(sub_obj.contains("value"), true);
  EXPECT_EQ(sub_obj.contains("alarm"), true);
  EXPECT_EQ(sub_obj.contains("timeStamp"), true);
  EXPECT_EQ(sub_obj.contains("display"), true);
  EXPECT_EQ(sub_obj.contains("control"), true);
  EXPECT_EQ(sub_obj.contains("valueAlarm"), true);
}

TEST(EpicsChannel, SerializationCAJSON) {
  INIT_CA_PROVIDER()
  EpicsChannelUPtr      pc;
  ConstDataUPtr         ser_value;
  ConstGetOperationUPtr get_op;

  EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_ca_provider, "variable:a"););
  EXPECT_NO_THROW(get_op = pc->get(););
  WHILE_OP(get_op, false);
  EXPECT_EQ(get_op->getState().event, pvac::GetEvent::Success);
  EXPECT_NO_THROW(ser_value = serialize(*get_op->getChannelData(), SerializationType::JSON)->data(););
  EXPECT_NE(ser_value, nullptr);
  EXPECT_NE(ser_value->data(), nullptr);
  EXPECT_NE(ser_value->size(), 0);
  std::string string_value(ser_value->data(), ser_value->size());
  // {"variable:sum":{"value":7E0,"alarm":{"severity":0,"status":0,"message":"NO_ALARM"},"timeStamp":{"secondsPastEpoch":1681018040,"nanoseconds":899757791,"userTag":0},"display":{"limitLow":0E0,"limitHigh":0E0,"description":"","units":"","precision":0,"form":{"index":0}},"control":{"limitLow":0E0,"limitHigh":0E0,"minStep":0E0},"valueAlarm":{"active":0,"lowAlarmLimit":"NaN","lowWarningLimit":"NaN","highWarningLimit":"NaN","highAlarmLimit":"NaN","lowAlarmSeverity":0,"lowWarningSeverity":0,"highWarningSeverity":0,"highAlarmSeverity":0,"hysteresis":0}}}
  boost::system::error_code ec;
  boost::json::value      jv;
  EXPECT_NO_THROW(jv = boost::json::parse(string_value, ec););
  EXPECT_EQ(ec.value(), false);
  EXPECT_EQ(jv.as_object().contains("variable:a"), true);
  auto sub_obj = jv.as_object().at("variable:a").as_object();
  EXPECT_EQ(sub_obj.contains("value"), true);
  EXPECT_EQ(sub_obj.contains("alarm"), true);
  EXPECT_EQ(sub_obj.contains("timeStamp"), true);
  EXPECT_EQ(sub_obj.contains("display"), true);
  EXPECT_EQ(sub_obj.contains("control"), true);
  EXPECT_EQ(sub_obj.contains("valueAlarm"), true);
}

TEST(EpicsChannel, SerializationCACompleteJSON) {
  INIT_CA_PROVIDER()
  EpicsChannelUPtr      pc;
  ConstDataUPtr         ser_value;
  ConstGetOperationUPtr get_op;

  EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_ca_provider, "variable:a"););
  // EXPECT_NO_THROW(pc->connect());
  EXPECT_NO_THROW(get_op = pc->get(););

  WHILE_OP(get_op, false);
  EXPECT_EQ(get_op->getState().event, pvac::GetEvent::Success);
  auto data = get_op->getChannelData();
  EXPECT_NO_THROW(ser_value = serialize(*data, SerializationType::JSON)->data(););
  EXPECT_NE(ser_value, nullptr);
  EXPECT_NE(ser_value->data(), nullptr);
  EXPECT_NE(ser_value->size(), 0);
  std::string string_value(ser_value->data(), ser_value->size());
  // {"variable:sum":{"value":7E0,"alarm":{"severity":0,"status":0,"message":"NO_ALARM"},"timeStamp":{"secondsPastEpoch":1681018040,"nanoseconds":899757791,"userTag":0},"display":{"limitLow":0E0,"limitHigh":0E0,"description":"","units":"","precision":0,"form":{"index":0}},"control":{"limitLow":0E0,"limitHigh":0E0,"minStep":0E0},"valueAlarm":{"active":0,"lowAlarmLimit":"NaN","lowWarningLimit":"NaN","highWarningLimit":"NaN","highAlarmLimit":"NaN","lowAlarmSeverity":0,"lowWarningSeverity":0,"highWarningSeverity":0,"highAlarmSeverity":0,"hysteresis":0}}}
  boost::system::error_code ec;
  boost::json::value      jv;
  EXPECT_NO_THROW(jv = boost::json::parse(string_value, ec););
  EXPECT_EQ(ec.value(), false);
  EXPECT_EQ(jv.as_object().contains("variable:a"), true);
  auto sub_obj = jv.as_object().at("variable:a").as_object();
  EXPECT_EQ(sub_obj.contains("value"), true);
  EXPECT_EQ(sub_obj.contains("alarm"), true);
  EXPECT_EQ(sub_obj.contains("timeStamp"), true);
  EXPECT_EQ(sub_obj.contains("display"), true);
  EXPECT_EQ(sub_obj.contains("control"), true);
  EXPECT_EQ(sub_obj.contains("valueAlarm"), true);
}

TEST(EpicsChannel, SerializationCACompleteJSONOnMonitor) {
  INIT_CA_PROVIDER()
  EpicsChannelUPtr             pc;
  ConstDataUPtr                ser_value;
  ConstMonitorOperationShrdPtr mon_op;

  EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_ca_provider, "variable:a"););
  // EXPECT_NO_THROW(pc->connect());
  EXPECT_NO_THROW(mon_op = pc->monitor(););
  WHILE_MONITOR(mon_op, !mon_op->hasData());
  auto evt_data = mon_op->getEventData();
  EXPECT_NO_THROW(ser_value = serialize(evt_data->event_data->at(0)->channel_data, SerializationType::JSON)->data(););
  EXPECT_NE(ser_value, nullptr);
  EXPECT_NE(ser_value->data(), nullptr);
  EXPECT_NE(ser_value->size(), 0);
  std::string string_value(ser_value->data(), ser_value->size());
  // {"variable:sum":{"value":7E0,"alarm":{"severity":0,"status":0,"message":"NO_ALARM"},"timeStamp":{"secondsPastEpoch":1681018040,"nanoseconds":899757791,"userTag":0},"display":{"limitLow":0E0,"limitHigh":0E0,"description":"","units":"","precision":0,"form":{"index":0}},"control":{"limitLow":0E0,"limitHigh":0E0,"minStep":0E0},"valueAlarm":{"active":0,"lowAlarmLimit":"NaN","lowWarningLimit":"NaN","highWarningLimit":"NaN","highAlarmLimit":"NaN","lowAlarmSeverity":0,"lowWarningSeverity":0,"highWarningSeverity":0,"highAlarmSeverity":0,"hysteresis":0}}}
  boost::system::error_code ec;
  boost::json::value      jv;
  EXPECT_NO_THROW(jv = boost::json::parse(string_value, ec););
  EXPECT_EQ(ec.value(), false);
  EXPECT_EQ(jv.as_object().contains("variable:a"), true);
  auto sub_obj = jv.as_object().at("variable:a").as_object();
  EXPECT_EQ(sub_obj.contains("value"), true);
  EXPECT_EQ(sub_obj.contains("alarm"), true);
  EXPECT_EQ(sub_obj.contains("timeStamp"), true);
  EXPECT_EQ(sub_obj.contains("display"), true);
  EXPECT_EQ(sub_obj.contains("control"), true);
  EXPECT_EQ(sub_obj.contains("valueAlarm"), true);
}

TEST(EpicsChannel, SerializationWaveformJSON) {
  INIT_PVA_PROVIDER()
  EpicsChannelUPtr      pc;
  ConstGetOperationUPtr get_op;
  ConstDataUPtr         ser_value;

  EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_pva_provider, "channel:waveform"););
  EXPECT_NO_THROW(get_op = pc->get(););
  WHILE_OP(get_op, false);
  EXPECT_NO_THROW(ser_value = serialize(*get_op->getChannelData(), SerializationType::JSON)->data(););
  EXPECT_NE(ser_value, nullptr);
  EXPECT_NE(ser_value->data(), nullptr);
  EXPECT_NE(ser_value->size(), 0);
  std::string string_value(ser_value->data(), ser_value->size());
  // {"variable:sum":{"value":[double,
  // double....],"alarm":{"severity":0,"status":0,"message":"NO_ALARM"},"timeStamp":{"secondsPastEpoch":1681018040,"nanoseconds":899757791,"userTag":0},"display":{"limitLow":0E0,"limitHigh":0E0,"description":"","units":"","precision":0,"form":{"index":0}},"control":{"limitLow":0E0,"limitHigh":0E0,"minStep":0E0},"valueAlarm":{"active":0,"lowAlarmLimit":"NaN","lowWarningLimit":"NaN","highWarningLimit":"NaN","highAlarmLimit":"NaN","lowAlarmSeverity":0,"lowWarningSeverity":0,"highWarningSeverity":0,"highAlarmSeverity":0,"hysteresis":0}}}
  boost::system::error_code ec;
  boost::json::value      jv;
  EXPECT_NO_THROW(jv = boost::json::parse(string_value, ec););
  EXPECT_EQ(ec.value(), false);
  EXPECT_EQ(jv.as_object().contains("channel:waveform"), true);
  auto sub_obj = jv.as_object().at("channel:waveform").as_object();
  EXPECT_EQ(sub_obj.contains("value"), true);
  EXPECT_EQ(sub_obj["value"].is_array(), true);
  EXPECT_EQ(sub_obj.contains("alarm"), true);
  EXPECT_EQ(sub_obj.contains("timeStamp"), true);
  EXPECT_EQ(sub_obj.contains("display"), true);
  EXPECT_EQ(sub_obj.contains("control"), true);
  EXPECT_EQ(sub_obj.contains("valueAlarm"), true);
}

TEST(EpicsChannel, SerializationCAWaveformJSON) {
  INIT_CA_PROVIDER()
  EpicsChannelUPtr      pc;
  ConstGetOperationUPtr get_op;
  ConstDataUPtr         ser_value;

  EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_ca_provider, "channel:waveform"););
  // EXPECT_NO_THROW(pc->connect());
  EXPECT_NO_THROW(get_op = pc->get(););
  WHILE_OP(get_op, false);
  EXPECT_NO_THROW(ser_value = serialize(*get_op->getChannelData(), SerializationType::JSON)->data(););
  EXPECT_NE(ser_value, nullptr);
  EXPECT_NE(ser_value->data(), nullptr);
  EXPECT_NE(ser_value->size(), 0);
  std::string string_value(ser_value->data(), ser_value->size());
  // {"variable:sum":{"value":[double,
  // double....],"alarm":{"severity":0,"status":0,"message":"NO_ALARM"},"timeStamp":{"secondsPastEpoch":1681018040,"nanoseconds":899757791,"userTag":0},"display":{"limitLow":0E0,"limitHigh":0E0,"description":"","units":"","precision":0,"form":{"index":0}},"control":{"limitLow":0E0,"limitHigh":0E0,"minStep":0E0},"valueAlarm":{"active":0,"lowAlarmLimit":"NaN","lowWarningLimit":"NaN","highWarningLimit":"NaN","highAlarmLimit":"NaN","lowAlarmSeverity":0,"lowWarningSeverity":0,"highWarningSeverity":0,"highAlarmSeverity":0,"hysteresis":0}}}
  boost::system::error_code ec;
  boost::json::value      jv;
  EXPECT_NO_THROW(jv = boost::json::parse(string_value, ec););
  EXPECT_EQ(ec.value(), false);
  EXPECT_EQ(jv.as_object().contains("channel:waveform"), true);
  auto sub_obj = jv.as_object().at("channel:waveform").as_object();
  EXPECT_EQ(sub_obj.contains("value"), true);
  EXPECT_EQ(sub_obj["value"].is_array(), true);
  EXPECT_EQ(sub_obj.contains("alarm"), true);
  EXPECT_EQ(sub_obj.contains("timeStamp"), true);
  EXPECT_EQ(sub_obj.contains("display"), true);
  // EXPECT_EQ(sub_obj.contains("control"), true);
  // EXPECT_EQ(sub_obj.contains("valueAlarm"), true);
}

typedef std::map<std::string, msgpack::object> MapStrMsgPackObj;
TEST(EpicsChannel, SerializationMsgpack) {
  INIT_PVA_PROVIDER()
  EpicsChannelUPtr      pc;
  ConstGetOperationUPtr get_op;
  ConstDataUPtr         ser_value;
  EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_pva_provider, "variable:sum"););
  // EXPECT_NO_THROW(pc->connect());
  EXPECT_NO_THROW(get_op = pc->get(););
  WHILE_OP(get_op, false);
  EXPECT_NO_THROW(ser_value = serialize(*get_op->getChannelData(), SerializationType::Msgpack)->data(););
  EXPECT_NE(ser_value, nullptr);
  EXPECT_NE(ser_value->data(), nullptr);
  EXPECT_NE(ser_value->size(), 0);
  msgpack::object_handle obj;
  EXPECT_NO_THROW(msgpack::unpack(obj, ser_value->data(), ser_value->size()););
  EXPECT_EQ(msgpack::type::MAP, obj->type);
  auto om = obj.get().as<MapStrMsgPackObj>();
  EXPECT_EQ(om.contains("variable:sum"), true);

  auto om_sub_1 = om["variable:sum"].as<MapStrMsgPackObj>();
  EXPECT_EQ(om_sub_1.contains("value"), true);
  EXPECT_EQ(om_sub_1.contains("alarm"), true);
  EXPECT_EQ(om_sub_1.contains("timeStamp"), true);
  EXPECT_EQ(om_sub_1.contains("display"), true);
  EXPECT_EQ(om_sub_1.contains("control"), true);
  EXPECT_EQ(om_sub_1.contains("valueAlarm"), true);
  //{"variable:sum":{"value":7,"alarm":{"severity":0,"status":0,"message":"NO_ALARM"},"timeStamp":{"secondsPastEpoch":1680995907,"nanoseconds":899753530,"userTag":0},"display":{"limitLow":0,"limitHigh":0,"description":"","units":"","precision":0,"form":{"index":0,"choices":"BIN(size:224)"}},"control":{"limitLow":0,"limitHigh":0,"minStep":0},"valueAlarm":{"active":0,"lowAlarmLimit":nan,"lowWarningLimit":nan,"highWarningLimit":nan,"highAlarmLimit":nan,"lowAlarmSeverity":0,"lowWarningSeverity":0,"highWarningSeverity":0,"highAlarmSeverity":0,"hysteresis":0}}}
}

TEST(EpicsChannel, SerializationCAMsgpack) {
  INIT_CA_PROVIDER()
  EpicsChannelUPtr      pc;
  ConstGetOperationUPtr get_op;
  ConstDataUPtr         ser_value;
  EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_ca_provider, "variable:sum"););
  // EXPECT_NO_THROW(pc->connect());
  EXPECT_NO_THROW(get_op = pc->get(););
  WHILE_OP(get_op, false);
  EXPECT_NO_THROW(ser_value = serialize(*get_op->getChannelData(), SerializationType::Msgpack)->data(););
  EXPECT_NE(ser_value, nullptr);
  EXPECT_NE(ser_value->data(), nullptr);
  EXPECT_NE(ser_value->size(), 0);
  msgpack::object_handle obj;
  EXPECT_NO_THROW(msgpack::unpack(obj, ser_value->data(), ser_value->size()););
  EXPECT_EQ(msgpack::type::MAP, obj->type);
  auto om = obj.get().as<MapStrMsgPackObj>();
  EXPECT_EQ(om.contains("variable:sum"), true);
  auto om_sub_1 = om["variable:sum"].as<MapStrMsgPackObj>();
  EXPECT_EQ(om_sub_1.contains("value"), true);
  EXPECT_EQ(om_sub_1.contains("alarm"), true);
  EXPECT_EQ(om_sub_1.contains("timeStamp"), true);
  EXPECT_EQ(om_sub_1.contains("display"), true);
  EXPECT_EQ(om_sub_1.contains("control"), true);
  EXPECT_EQ(om_sub_1.contains("valueAlarm"), true);
  //{"variable:sum":{"value":7,"alarm":{"severity":0,"status":0,"message":"NO_ALARM"},"timeStamp":{"secondsPastEpoch":1680995907,"nanoseconds":899753530,"userTag":0},"display":{"limitLow":0,"limitHigh":0,"description":"","units":"","precision":0,"form":{"index":0,"choices":"BIN(size:224)"}},"control":{"limitLow":0,"limitHigh":0,"minStep":0},"valueAlarm":{"active":0,"lowAlarmLimit":nan,"lowWarningLimit":nan,"highWarningLimit":nan,"highAlarmLimit":nan,"lowAlarmSeverity":0,"lowWarningSeverity":0,"highWarningSeverity":0,"highAlarmSeverity":0,"hysteresis":0}}}
}

TEST(EpicsChannel, SerializationMsgpackWaveform) {
  INIT_PVA_PROVIDER()
  EpicsChannelUPtr      pc;
  ConstGetOperationUPtr get_op;
  ConstDataUPtr         ser_value;
  EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_pva_provider, "channel:waveform"););
  // EXPECT_NO_THROW(pc->connect());
  EXPECT_NO_THROW(get_op = pc->get(););
  WHILE_OP(get_op, false);
  EXPECT_NO_THROW(ser_value = serialize(*get_op->getChannelData(), SerializationType::Msgpack)->data(););
  EXPECT_NE(ser_value, nullptr);
  EXPECT_NE(ser_value->data(), nullptr);
  EXPECT_NE(ser_value->size(), 0);
  msgpack::object_handle obj;
  EXPECT_NO_THROW(msgpack::unpack(obj, ser_value->data(), ser_value->size()););
  EXPECT_EQ(msgpack::type::MAP, obj->type);
  auto om = obj.get().as<MapStrMsgPackObj>();
  EXPECT_EQ(om.contains("channel:waveform"), true);
  auto om_sub_1 = om["channel:waveform"].as<MapStrMsgPackObj>();
  EXPECT_EQ(om_sub_1.contains("value"), true);
  EXPECT_EQ(om_sub_1["value"].type, msgpack::type::ARRAY);
  EXPECT_EQ(om_sub_1.contains("alarm"), true);
  EXPECT_EQ(om_sub_1.contains("timeStamp"), true);
  EXPECT_EQ(om_sub_1.contains("display"), true);
  EXPECT_EQ(om_sub_1.contains("control"), true);
  EXPECT_EQ(om_sub_1.contains("valueAlarm"), true);
  //{"variable:sum":{"value":[2,3,4,5,6,7,8],"alarm":{"severity":0,"status":0,"message":"NO_ALARM"},"timeStamp":{"secondsPastEpoch":1680995907,"nanoseconds":899753530,"userTag":0},"display":{"limitLow":0,"limitHigh":0,"description":"","units":"","precision":0,"form":{"index":0,"choices":"BIN(size:224)"}},"control":{"limitLow":0,"limitHigh":0,"minStep":0},"valueAlarm":{"active":0,"lowAlarmLimit":nan,"lowWarningLimit":nan,"highWarningLimit":nan,"highAlarmLimit":nan,"lowAlarmSeverity":0,"lowWarningSeverity":0,"highWarningSeverity":0,"highAlarmSeverity":0,"hysteresis":0}}}
}

TEST(EpicsChannel, SerializationNTTableMsgpack) {
  INIT_PVA_PROVIDER()
  EpicsChannelUPtr      pc;
  ConstGetOperationUPtr get_op;
  ConstDataUPtr         ser_value;
  EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_pva_provider, "K2EG:TEST:TWISS"););
  // EXPECT_NO_THROW(pc->connect());
  EXPECT_NO_THROW(get_op = pc->get(););
  WHILE_OP(get_op, false);
  EXPECT_NO_THROW(ser_value = serialize(*get_op->getChannelData(), SerializationType::Msgpack)->data(););
  EXPECT_NE(ser_value, nullptr);
  EXPECT_NE(ser_value->data(), nullptr);
  EXPECT_NE(ser_value->size(), 0);
  msgpack::object_handle obj;
  EXPECT_NO_THROW(obj = msgpack::unpack(ser_value->data(), ser_value->size()););
  EXPECT_EQ(msgpack::type::MAP, obj->type);
  auto om = obj.get().as<MapStrMsgPackObj>();
  EXPECT_EQ(om.contains("K2EG:TEST:TWISS"), true);
  auto om_sub_1 = om["K2EG:TEST:TWISS"].as<MapStrMsgPackObj>();
  EXPECT_EQ(om_sub_1.contains("labels"), true);
  EXPECT_EQ(om_sub_1.contains("value"), true);
  EXPECT_EQ(om_sub_1.contains("descriptor"), true);
  EXPECT_EQ(om_sub_1.contains("alarm"), true);
  EXPECT_EQ(om_sub_1.contains("timeStamp"), true);
}

TEST(EpicsChannel, SerializationNTNDArrayMsgpack) {
  INIT_PVA_PROVIDER()
  EpicsChannelUPtr      pc;
  ConstGetOperationUPtr get_op;
  ConstDataUPtr         ser_value;
  EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_pva_provider, "K2EG:TEST:IMAGE"););
  // EXPECT_NO_THROW(pc->connect());
  EXPECT_NO_THROW(get_op = pc->get(););
  WHILE_OP(get_op, false);
  EXPECT_NO_THROW(ser_value = serialize(*get_op->getChannelData(), SerializationType::Msgpack)->data(););
  EXPECT_NE(ser_value, nullptr);
  EXPECT_NE(ser_value->data(), nullptr);
  EXPECT_NE(ser_value->size(), 0);
  msgpack::object_handle obj;
  EXPECT_NO_THROW(obj = msgpack::unpack(ser_value->data(), ser_value->size()););
  EXPECT_EQ(msgpack::type::MAP, obj->type);
  auto om = obj.get().as<MapStrMsgPackObj>();
  EXPECT_EQ(om.contains("K2EG:TEST:IMAGE"), true);
  auto om_sub_1 = om["K2EG:TEST:IMAGE"].as<MapStrMsgPackObj>();
  EXPECT_EQ(om_sub_1.contains("value"), true);
  EXPECT_EQ(om_sub_1.contains("compressedSize"), true);
  EXPECT_EQ(om_sub_1.contains("uncompressedSize"), true);
  EXPECT_EQ(om_sub_1.contains("uniqueId"), true);
  EXPECT_EQ(om_sub_1.contains("timeStamp"), true);
  EXPECT_EQ(om_sub_1.contains("dimension"), true);
  EXPECT_EQ(om_sub_1.contains("attribute"), true);
}

TEST(EpicsChannel, SerializationMsgpackCompact) {
  INIT_PVA_PROVIDER()
  EpicsChannelUPtr      pc;
  ConstGetOperationUPtr get_op;
  ConstDataUPtr         ser_value;
  EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_pva_provider, "variable:sum"););
  // EXPECT_NO_THROW(pc->connect());
  EXPECT_NO_THROW(get_op = pc->get(););
  WHILE_OP(get_op, false);
  EXPECT_NO_THROW(ser_value = serialize(*get_op->getChannelData(), SerializationType::MsgpackCompact)->data(););
  EXPECT_NE(ser_value, nullptr);
  EXPECT_NE(ser_value->data(), nullptr);
  EXPECT_NE(ser_value->size(), 0);
  msgpack::object_handle obj;
  EXPECT_NO_THROW(obj = msgpack::unpack(ser_value->data(), ser_value->size()););
  EXPECT_EQ(msgpack::type::ARRAY, obj->type);
  auto object_vector = obj.get().as<MsgpackObjectVector>();
  EXPECT_EQ(object_vector.size(), 28);
  EXPECT_EQ(object_vector[0].type, msgpack::type::STR);
  EXPECT_EQ(object_vector[1].type, msgpack::type::FLOAT);
  //["variable:sum",7,0,0,"NO_ALARM",1681706068,208836822,0,0,0,"","",0,0,["Default","String","Binary","Decimal","Hex","Exponential","Engineering"],0,0,0,0,nan,nan,nan,nan,0,0,0,0,0]
}

TEST(EpicsChannel, SerializationCAMsgpackCompact) {
  INIT_CA_PROVIDER()
  EpicsChannelUPtr      pc;
  ConstGetOperationUPtr get_op;
  ConstDataUPtr         ser_value;
  EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_ca_provider, "variable:sum"););
  // EXPECT_NO_THROW(pc->connect());
  EXPECT_NO_THROW(get_op = pc->get(););
  WHILE_OP(get_op, false);
  EXPECT_NO_THROW(ser_value = serialize(*get_op->getChannelData(), SerializationType::MsgpackCompact)->data(););
  EXPECT_NE(ser_value, nullptr);
  EXPECT_NE(ser_value->data(), nullptr);
  EXPECT_NE(ser_value->size(), 0);
  msgpack::object_handle obj;
  EXPECT_NO_THROW(obj = msgpack::unpack(ser_value->data(), ser_value->size()););
  EXPECT_EQ(msgpack::type::ARRAY, obj->type);
  auto object_vector = obj.get().as<MsgpackObjectVector>();
  EXPECT_EQ(object_vector.size(), 26);
  EXPECT_EQ(object_vector[0].type, msgpack::type::STR);
  EXPECT_EQ(object_vector[1].type, msgpack::type::FLOAT);
  //["variable:sum",7,0,0,"NO_ALARM",1681706068,208836822,0,0,0,"","",0,0,["Default","String","Binary","Decimal","Hex","Exponential","Engineering"],0,0,0,0,nan,nan,nan,nan,0,0,0,0,0]
}

typedef std::vector<msgpack::object> MsgpackObjectVector;
TEST(EpicsChannel, SerializationMsgpackCompactWaveform) {
  INIT_PVA_PROVIDER()
  EpicsChannelUPtr      pc;
  ConstGetOperationUPtr get_op;
  ConstDataUPtr         ser_value;
  EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_pva_provider, "channel:waveform"););
  EXPECT_NO_THROW(get_op = pc->get(););
  WHILE_OP(get_op, false);
  EXPECT_NO_THROW(ser_value = serialize(*get_op->getChannelData(), SerializationType::MsgpackCompact)->data(););
  EXPECT_NE(ser_value, nullptr);
  EXPECT_NE(ser_value->data(), nullptr);
  EXPECT_NE(ser_value->size(), 0);
  msgpack::object_handle obj;
  EXPECT_NO_THROW(obj = msgpack::unpack(ser_value->data(), ser_value->size()););
  EXPECT_EQ(msgpack::type::ARRAY, obj->type);
  auto object_vector = obj.get().as<MsgpackObjectVector>();
  EXPECT_EQ(object_vector.size(), 28);
  EXPECT_EQ(object_vector[0].type, msgpack::type::STR);
  EXPECT_EQ(object_vector[1].type, msgpack::type::ARRAY);
  //["variable:sum",7,0,0,"NO_ALARM",1681706068,208836822,0,0,0,"","",0,0,["Default","String","Binary","Decimal","Hex","Exponential","Engineering"],0,0,0,0,nan,nan,nan,nan,0,0,0,0,0]
}