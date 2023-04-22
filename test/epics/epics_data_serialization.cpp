#include <gtest/gtest.h>
#include <k2eg/service/epics/EpicsChannel.h>
#include <k2eg/service/epics/EpicsServiceManager.h>

#include <boost/json.hpp>
#include <cstddef>
#include <msgpack.hpp>
#include <sstream>

#include "epics.h"
#include "msgpack/v3/object_fwd_decl.hpp"

using namespace k2eg::service::epics_impl;

TEST(Epics, SerializationJSON) {
  INIT_PVA_PROVIDER()
  EpicsChannelUPtr              pc;
  ConstChannelDataUPtr          value;
  ConstSerializedMessageShrdPtr ser_value;

  EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_pva_provider, "variable:sum"););
  // EXPECT_NO_THROW(pc->connect());
  EXPECT_NO_THROW(value = pc->getChannelData(););
  EXPECT_NO_THROW(ser_value = serialize(*value, SerializationType::JSON););
  EXPECT_NE(ser_value, nullptr);
  EXPECT_NE(ser_value->data(), nullptr);
  EXPECT_NE(ser_value->size(), 0);
  std::string string_value(ser_value->data(), ser_value->size());
  // {"variable:sum":{"value":7E0,"alarm":{"severity":0,"status":0,"message":"NO_ALARM"},"timeStamp":{"secondsPastEpoch":1681018040,"nanoseconds":899757791,"userTag":0},"display":{"limitLow":0E0,"limitHigh":0E0,"description":"","units":"","precision":0,"form":{"index":0}},"control":{"limitLow":0E0,"limitHigh":0E0,"minStep":0E0},"valueAlarm":{"active":0,"lowAlarmLimit":"NaN","lowWarningLimit":"NaN","highWarningLimit":"NaN","highAlarmLimit":"NaN","lowAlarmSeverity":0,"lowWarningSeverity":0,"highWarningSeverity":0,"highAlarmSeverity":0,"hysteresis":0}}}
  boost::json::error_code ec;
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

TEST(Epics, SerializationWaveformJSON) {
  INIT_PVA_PROVIDER()
  EpicsChannelUPtr              pc;
  ConstChannelDataUPtr          value;
  ConstSerializedMessageShrdPtr ser_value;

  EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_pva_provider, "channel:waveform"););
  // EXPECT_NO_THROW(pc->connect());
  EXPECT_NO_THROW(value = pc->getChannelData(););
  EXPECT_NO_THROW(ser_value = serialize(*value, SerializationType::JSON););
  EXPECT_NE(ser_value, nullptr);
  EXPECT_NE(ser_value->data(), nullptr);
  EXPECT_NE(ser_value->size(), 0);
  std::string string_value(ser_value->data(), ser_value->size());
  // {"variable:sum":{"value":[double,
  // double....],"alarm":{"severity":0,"status":0,"message":"NO_ALARM"},"timeStamp":{"secondsPastEpoch":1681018040,"nanoseconds":899757791,"userTag":0},"display":{"limitLow":0E0,"limitHigh":0E0,"description":"","units":"","precision":0,"form":{"index":0}},"control":{"limitLow":0E0,"limitHigh":0E0,"minStep":0E0},"valueAlarm":{"active":0,"lowAlarmLimit":"NaN","lowWarningLimit":"NaN","highWarningLimit":"NaN","highAlarmLimit":"NaN","lowAlarmSeverity":0,"lowWarningSeverity":0,"highWarningSeverity":0,"highAlarmSeverity":0,"hysteresis":0}}}
  boost::json::error_code ec;
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

typedef std::map<std::string, msgpack::object> MapStrMsgPackObj;
TEST(Epics, SerializationMsgpack) {
  INIT_PVA_PROVIDER()
  EpicsChannelUPtr              pc;
  ConstChannelDataUPtr          value;
  ConstSerializedMessageShrdPtr ser_value;
  EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_pva_provider, "variable:sum"););
  // EXPECT_NO_THROW(pc->connect());
  EXPECT_NO_THROW(value = pc->getChannelData(););
  EXPECT_NO_THROW(ser_value = serialize(*value, SerializationType::Msgpack););
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

TEST(Epics, SerializationMsgpackWaveform) {
  INIT_PVA_PROVIDER()
  EpicsChannelUPtr              pc;
  ConstChannelDataUPtr          value;
  ConstSerializedMessageShrdPtr ser_value;
  EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_pva_provider, "channel:waveform"););
  // EXPECT_NO_THROW(pc->connect());
  EXPECT_NO_THROW(value = pc->getChannelData(););
  EXPECT_NO_THROW(ser_value = serialize(*value, SerializationType::Msgpack););
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

typedef std::vector<msgpack::object> MsgpackObjectVector;
TEST(Epics, SerializationMsgpackCompact) {
  INIT_PVA_PROVIDER()
  EpicsChannelUPtr              pc;
  ConstChannelDataUPtr          value;
  ConstSerializedMessageShrdPtr ser_value;
  EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_pva_provider, "variable:sum"););
  // EXPECT_NO_THROW(pc->connect());
  EXPECT_NO_THROW(value = pc->getChannelData(););
  EXPECT_NO_THROW(ser_value = serialize(*value, SerializationType::MsgpackCompact););
  EXPECT_NE(ser_value, nullptr);
  EXPECT_NE(ser_value->data(), nullptr);
  EXPECT_NE(ser_value->size(), 0);
  msgpack::object_handle obj;
  EXPECT_NO_THROW(obj = msgpack::unpack(ser_value->data(), ser_value->size()););
  EXPECT_EQ(msgpack::type::ARRAY, obj->type);
  auto object_vector = obj.get().as<MsgpackObjectVector>();
  EXPECT_EQ(object_vector.size(), 28);
  EXPECT_EQ(object_vector[0].type, msgpack::type::STR);
  EXPECT_EQ(object_vector[1].type, msgpack::type::POSITIVE_INTEGER);
  //["variable:sum",7,0,0,"NO_ALARM",1681706068,208836822,0,0,0,"","",0,0,["Default","String","Binary","Decimal","Hex","Exponential","Engineering"],0,0,0,0,nan,nan,nan,nan,0,0,0,0,0]
}

typedef std::vector<msgpack::object> MsgpackObjectVector;
TEST(Epics, SerializationMsgpackCompactWaveform) {
  INIT_PVA_PROVIDER()
  EpicsChannelUPtr              pc;
  ConstChannelDataUPtr          value;
  ConstSerializedMessageShrdPtr ser_value;
  EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_pva_provider, "channel:waveform"););
  // EXPECT_NO_THROW(pc->connect());
  EXPECT_NO_THROW(value = pc->getChannelData(););
  EXPECT_NO_THROW(ser_value = serialize(*value, SerializationType::MsgpackCompact););
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