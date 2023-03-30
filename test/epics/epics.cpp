#include <chrono>
#include <k2eg/service/epics/EpicsChannel.h>
#include <k2eg/service/epics/EpicsServiceManager.h>
#include <gtest/gtest.h>
#include <thread>

using namespace k2eg::service::epics_impl;

TEST(Epics, ChannelFault) {
    EpicsChannelUPtr pc;
    EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>("ca", "bacd_channel_name"));
}

TEST(Epics, ChannelOK) {
    EpicsChannelUPtr pc;
    epics::pvData::PVStructure::const_shared_pointer val;
    EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>("pva", "variable:sum"););
    EXPECT_NO_THROW(pc->connect());
    EXPECT_NO_THROW(val = pc->getData(););
    EXPECT_NE(val, nullptr);
}

TEST(Epics, ChannelOKWithAddress) {
    EpicsChannelUPtr pc;
    epics::pvData::PVStructure::const_shared_pointer val;
    EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>("pva", "variable:sum", "epics"););
    EXPECT_NO_THROW(pc->connect());
    EXPECT_NO_THROW(val = pc->getData(););
    EXPECT_NE(val, nullptr);
}

TEST(Epics, ChannelOKSerialize) {
    EpicsChannelUPtr pc;
    ConstChannelDataUPtr val;
    EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>("pva", "variable:sum", "epics"););
    EXPECT_NO_THROW(pc->connect());
    EXPECT_NO_THROW(val = pc->getChannelData(););
    std::string json = serialize(*val, SerializationType::JSON);
    const char* s = json.c_str();
    int size = json.size();
    EXPECT_NE(val, nullptr);
}

bool retry_eq(const EpicsChannel& channel, const std::string& name, double value, int mseconds, int retry_times) {
    for (int times = retry_times; times != 0; times--) {
        auto val = channel.getData();
        if (val->getSubField<epics::pvData::PVDouble>(name)->get() == value) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(mseconds));
    }
    return false;
}

TEST(Epics, ChannelGetSetTemplatedGet) {
    EpicsChannelUPtr pc_sum;
    EpicsChannelUPtr pc_a;
    EpicsChannelUPtr pc_b;
    epics::pvData::PVStructure::const_shared_pointer val;
    EXPECT_NO_THROW(pc_sum = std::make_unique<EpicsChannel>("pva", "variable:sum"););
    EXPECT_NO_THROW(pc_a = std::make_unique<EpicsChannel>("pva", "variable:a"););
    EXPECT_NO_THROW(pc_b = std::make_unique<EpicsChannel>("pva", "variable:b"););
    EXPECT_NO_THROW(pc_sum->connect());
    EXPECT_NO_THROW(pc_a->connect());
    EXPECT_NO_THROW(pc_b->connect());
    EXPECT_NO_THROW(pc_a->putData<int32_t>("value", 0););
    EXPECT_NO_THROW(pc_b->putData<int32_t>("value", 0););
    EXPECT_EQ(retry_eq(*pc_sum, "value", 0, 500, 3), true);
    EXPECT_NO_THROW(pc_a->putData<int32_t>("value", 5););
    EXPECT_NO_THROW(pc_b->putData<int32_t>("value", 5););
    EXPECT_EQ(retry_eq(*pc_sum, "value", 10, 500, 3), true);
}

TEST(Epics, ChannelGetSetPVDatadGet) {
    EpicsChannelUPtr pc_sum;
    EpicsChannelUPtr pc_a;
    EpicsChannelUPtr pc_b;
    epics::pvData::PVStructure::const_shared_pointer val;
    EXPECT_NO_THROW(pc_sum = std::make_unique<EpicsChannel>("pva", "variable:sum"););
    EXPECT_NO_THROW(pc_a = std::make_unique<EpicsChannel>("pva", "variable:a"););
    EXPECT_NO_THROW(pc_b = std::make_unique<EpicsChannel>("pva", "variable:b"););
    EXPECT_NO_THROW(pc_sum->connect());
    EXPECT_NO_THROW(pc_a->connect());
    EXPECT_NO_THROW(pc_b->connect());
    EXPECT_NO_THROW(pc_a->putData("value", epics::pvData::AnyScalar(0)););
    EXPECT_NO_THROW(pc_b->putData("value", epics::pvData::AnyScalar(0)););
    EXPECT_EQ(retry_eq(*pc_sum, "value", 0, 500, 3), true);
    EXPECT_NO_THROW(pc_a->putData("value", epics::pvData::AnyScalar(5)););
    EXPECT_NO_THROW(pc_b->putData("value", epics::pvData::AnyScalar(5)););
    EXPECT_EQ(retry_eq(*pc_sum, "value", 10, 500, 3), true);
}

TEST(Epics, ChannelMonitor) {
    EpicsChannelUPtr pc_a;
    epics::pvData::PVStructure::const_shared_pointer val;
    EXPECT_NO_THROW(pc_a = std::make_unique<EpicsChannel>("pva", "variable:a"););
    EXPECT_NO_THROW(pc_a->connect());
    // enable monitor
    EXPECT_NO_THROW(pc_a->putData("value", epics::pvData::AnyScalar(0)););
    EXPECT_EQ(retry_eq(*pc_a, "value", 0, 500, 3), true);

    EXPECT_NO_THROW(pc_a->startMonitor(););
    MonitorEventVecShrdPtr fetched = pc_a->monitor();
    EXPECT_EQ(fetched->size(), 1);
    EXPECT_EQ(fetched->at(0)->type, MonitorType::Data);
    EXPECT_EQ(fetched->at(0)->channel_data.data->getSubField<epics::pvData::PVDouble>("value")->get(), 0);

    EXPECT_NO_THROW(pc_a->putData("value", epics::pvData::AnyScalar(1)););
    EXPECT_NO_THROW(pc_a->putData("value", epics::pvData::AnyScalar(2)););

    std::this_thread::sleep_for(std::chrono::seconds(1));

    fetched = pc_a->monitor();
    EXPECT_EQ(fetched->size(), 2);
    EXPECT_EQ(fetched->at(0)->type, MonitorType::Data);
    EXPECT_EQ(fetched->at(0)->channel_data.data->getSubField<epics::pvData::PVDouble>("value")->get(), 1);
    EXPECT_EQ(fetched->at(1)->type, MonitorType::Data);
    EXPECT_EQ(fetched->at(1)->channel_data.data->getSubField<epics::pvData::PVDouble>("value")->get(), 2);
    EXPECT_NO_THROW(pc_a->stopMonitor(););
}

MonitorEventVec allEvent;
std::mutex check_result_mutex;
void handler(const MonitorEventVecShrdPtr& event_data) {
    std::lock_guard guard(check_result_mutex);
    allEvent.insert(allEvent.end(), (*event_data).begin(), (*event_data).end());
}

TEST(Epics, EpicsServiceManager) {
    allEvent.clear();
    k2eg::common::BroadcastToken handler_tok;
    std::unique_ptr<EpicsServiceManager> monitor = std::make_unique<EpicsServiceManager>();
    EXPECT_NO_THROW(handler_tok = monitor->addHandler(std::bind(handler, std::placeholders::_1)););
    EXPECT_NO_THROW(monitor->addChannel("channel:ramp:ramp"););
    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_EQ(allEvent.size() > 0, true);
    monitor.reset();
}

TEST(Epics, EpicsServiceManagerWrongMOnitoredDevices) {
    allEvent.clear();
    std::unique_ptr<EpicsServiceManager> monitor = std::make_unique<EpicsServiceManager>();
    EXPECT_NO_THROW(monitor->addHandler(std::bind(handler, std::placeholders::_1)););
    EXPECT_ANY_THROW(monitor->addChannel("ca", "wrong::device"););
    auto vec = monitor->getMonitoredChannels();
    std::stringstream ss;
    for (auto& e: vec) {
        ss << e;
    }
    EXPECT_EQ(monitor->getChannelMonitoredSize(), 0) << "[" + ss.str() + "]";
    monitor.reset();
}

TEST(Epics, EpicsServiceManagerAddRemove) {
    allEvent.clear();
    std::unique_ptr<EpicsServiceManager> monitor = std::make_unique<EpicsServiceManager>();
    EXPECT_NO_THROW(monitor->addChannel("wrong::device", "pva"););
    EXPECT_NO_THROW(monitor->removeChannel("wrong::device"););
    EXPECT_EQ(monitor->getChannelMonitoredSize(), 0);
    monitor.reset();
}

TEST(Epics, EpicsServiceManagerRemoveHandler) {
    k2eg::common::BroadcastToken handler_tok;
    std::unique_ptr<EpicsServiceManager> monitor = std::make_unique<EpicsServiceManager>();
    EXPECT_NO_THROW(handler_tok = monitor->addHandler(std::bind(handler, std::placeholders::_1)););
    EXPECT_EQ(monitor->getHandlerSize(), 1);
    handler_tok.reset();                     // this should invalidate the handler within the manager
    EXPECT_EQ(monitor->getHandlerSize(), 0); // this should be 0 because of handler_tok.reset()
    monitor.reset();
}