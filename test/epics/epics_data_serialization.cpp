#include <k2eg/service/epics/EpicsChannel.h>
#include <k2eg/service/epics/EpicsServiceManager.h>
#include <gtest/gtest.h>
#include <msgpack.hpp>
#include <sstream>

using namespace k2eg::service::epics_impl;

TEST(Epics, SerializationJSON) {
    EpicsChannelUPtr pc;
    ConstChannelDataUPtr value;
    ConstSerializedMessageUPtr ser_value;

    EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>("pva", "variable:sum"););
    EXPECT_NO_THROW(pc->connect());
    EXPECT_NO_THROW(value = pc->getChannelData(););
    EXPECT_NO_THROW(ser_value = serialize(*value, SerializationType::JSON););
    EXPECT_NE(ser_value, nullptr);
    EXPECT_NE(ser_value->data(), nullptr);
    EXPECT_NE(ser_value->size(), 0);
    std::string string_value(ser_value->data(), ser_value->size());
    // {"variable:sum":{{"value": 7,"alarm": {"severity": 0,"status": 0,"message": "NO_ALARM"}}}
    EXPECT_NE(string_value.find("variable:sum"), -1);
    std::cout << string_value << std::endl;
}

TEST(Epics, SerializationMsgpack) {
    EpicsChannelUPtr pc;
    ConstChannelDataUPtr value;
    ConstSerializedMessageUPtr ser_value;
    msgpack::object_handle result;
    std::size_t off = 0;
    EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>("pva", "variable:sum"););
    EXPECT_NO_THROW(pc->connect());
    EXPECT_NO_THROW(value = pc->getChannelData(););
    EXPECT_NO_THROW(ser_value = serialize(*value, SerializationType::MsgPack););
    EXPECT_NE(ser_value, nullptr);
    EXPECT_NE(ser_value->data(), nullptr);
    EXPECT_NE(ser_value->size(), 0);
    EXPECT_NO_THROW(msgpack::unpack(result, ser_value->data(), ser_value->size(), off););
    EXPECT_NO_THROW(msgpack::object obj(result.get()););
    //{"variable:sum":{"value":7,"alarm":{"severity":0,"status":0,"message":"NO_ALARM"},"timeStamp":{"secondsPastEpoch":1680995907,"nanoseconds":899753530,"userTag":0},"display":{"limitLow":0,"limitHigh":0,"description":"","units":"","precision":0,"form":{"index":0,"choices":"BIN(size:224)"}},"control":{"limitLow":0,"limitHigh":0,"minStep":0},"valueAlarm":{"active":0,"lowAlarmLimit":nan,"lowWarningLimit":nan,"highWarningLimit":nan,"highAlarmLimit":nan,"lowAlarmSeverity":0,"lowWarningSeverity":0,"highWarningSeverity":0,"highAlarmSeverity":0,"hysteresis":0}}}
    EXPECT_EQ(off, 678);
}
