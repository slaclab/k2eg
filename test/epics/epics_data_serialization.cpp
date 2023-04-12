#include <k2eg/service/epics/EpicsChannel.h>
#include <k2eg/service/epics/EpicsServiceManager.h>
#include <gtest/gtest.h>
#include <msgpack.hpp>
#include <sstream>

using namespace k2eg::service::epics_impl;

TEST(Epics, SerializationJSON) {
    EpicsChannelUPtr pc;
    ConstChannelDataUPtr value;
    ConstSerializedMessageShrdPtr ser_value;

    EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>("pva", "variable:sum"););
    EXPECT_NO_THROW(pc->connect());
    EXPECT_NO_THROW(value = pc->getChannelData(););
    EXPECT_NO_THROW(ser_value = serialize(*value, SerializationType::JSON););
    EXPECT_NE(ser_value, nullptr);
    EXPECT_NE(ser_value->data(), nullptr);
    EXPECT_NE(ser_value->size(), 0);
    std::string string_value(ser_value->data(), ser_value->size());
    // {"variable:sum":{"value":7E0,"alarm":{"severity":0,"status":0,"message":"NO_ALARM"},"timeStamp":{"secondsPastEpoch":1681018040,"nanoseconds":899757791,"userTag":0},"display":{"limitLow":0E0,"limitHigh":0E0,"description":"","units":"","precision":0,"form":{"index":0}},"control":{"limitLow":0E0,"limitHigh":0E0,"minStep":0E0},"valueAlarm":{"active":0,"lowAlarmLimit":"NaN","lowWarningLimit":"NaN","highWarningLimit":"NaN","highAlarmLimit":"NaN","lowAlarmSeverity":0,"lowWarningSeverity":0,"highWarningSeverity":0,"highAlarmSeverity":0,"hysteresis":0}}}
    EXPECT_NE(string_value.find("variable:sum"), -1);
}

TEST(Epics, SerializationMsgpack) {
    EpicsChannelUPtr pc;
    ConstChannelDataUPtr value;
    ConstSerializedMessageShrdPtr ser_value;
    msgpack::object_handle result;
    EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>("pva", "variable:sum"););
    EXPECT_NO_THROW(pc->connect());
    EXPECT_NO_THROW(value = pc->getChannelData(););
    EXPECT_NO_THROW(ser_value = serialize(*value, SerializationType::MsgPack););
    EXPECT_NE(ser_value, nullptr);
    EXPECT_NE(ser_value->data(), nullptr);
    EXPECT_NE(ser_value->size(), 0);
    msgpack::unpacked msg_upacked;
    EXPECT_NO_THROW(msgpack::unpack(msg_upacked, ser_value->data(), ser_value->size()););
    EXPECT_NO_THROW(msgpack::object obj(result.get()););
    //{"variable:sum":{"value":7,"alarm":{"severity":0,"status":0,"message":"NO_ALARM"},"timeStamp":{"secondsPastEpoch":1680995907,"nanoseconds":899753530,"userTag":0},"display":{"limitLow":0,"limitHigh":0,"description":"","units":"","precision":0,"form":{"index":0,"choices":"BIN(size:224)"}},"control":{"limitLow":0,"limitHigh":0,"minStep":0},"valueAlarm":{"active":0,"lowAlarmLimit":nan,"lowWarningLimit":nan,"highWarningLimit":nan,"highAlarmLimit":nan,"lowAlarmSeverity":0,"lowWarningSeverity":0,"highWarningSeverity":0,"highAlarmSeverity":0,"hysteresis":0}}}
}
