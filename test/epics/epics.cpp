#include <chrono>
#include <k2eg/service/epics/EpicsChannel.h>
#include <k2eg/service/epics/EpicsServiceManager.h>
#include <gtest/gtest.h>
#include <unistd.h>
#include <thread>
#include "k2eg/service/epics/EpicsGetOperation.h"
#include "k2eg/service/epics/EpicsPutOperation.h"
#include "epics.h"
#include "pvData.h"

using namespace k2eg::service::epics_impl;

#define WHILE(x,v) \
do{std::this_thread::sleep_for(std::chrono::milliseconds(250));}while(x == v)

TEST(Epics, ChannelFault) {
    INIT_CA_PROVIDER()
    EpicsChannelUPtr pc;
    EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_ca_provider, "bacd_channel_name"));
}

TEST(Epics, ChannelOK) {
    INIT_PVA_PROVIDER()
    EpicsChannelUPtr pc;
    epics::pvData::PVStructure::const_shared_pointer val;
    EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_pva_provider, "variable:sum"););
    // EXPECT_NO_THROW(pc->connect());
    EXPECT_NO_THROW(val = pc->getData(););
    EXPECT_NE(val, nullptr);
}

TEST(Epics, ChannelOKWithAddress) {
     INIT_PVA_PROVIDER()
    EpicsChannelUPtr pc;
    epics::pvData::PVStructure::const_shared_pointer val;
    EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_pva_provider, "variable:sum", "epics"););
    // EXPECT_NO_THROW(pc->connect());
    EXPECT_NO_THROW(val = pc->getData(););
    EXPECT_NE(val, nullptr);
}

TEST(Epics, ChannelGetOpOk) {
    INIT_PVA_PROVIDER()
    EpicsChannelUPtr pc;
    ConstGetOperationUPtr get_op;
    epics::pvData::PVStructure::const_shared_pointer val;
    EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_pva_provider, "variable:sum"););
    // EXPECT_NO_THROW(pc->connect());
    EXPECT_NO_THROW(get_op = pc->get(););
    WHILE(get_op->isDone(), false);
    EXPECT_EQ(get_op->getState().event, pvac::GetEvent::Success);
}

bool retry_eq(const EpicsChannel& channel, const std::string& name, double value, int mseconds, int retry_times) {
    for (int times = retry_times; times != 0; times--) {
        auto val = channel.getData();
        auto dval = val->getSubField<epics::pvData::PVDouble>(name)->get();
        if (dval == value) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(mseconds));
    }
    return false;
}

TEST(Epics, ChannelPutValue) {
     INIT_PVA_PROVIDER()
    ConstPutOperationUPtr put_op_a;
    ConstPutOperationUPtr put_op_b;
    EpicsChannelUPtr pc_sum;
    EpicsChannelUPtr pc_a;
    EpicsChannelUPtr pc_b;
    epics::pvData::PVStructure::const_shared_pointer val;
    EXPECT_NO_THROW(pc_sum = std::make_unique<EpicsChannel>(*test_pva_provider, "variable:sum"););
    EXPECT_NO_THROW(pc_a = std::make_unique<EpicsChannel>(*test_pva_provider, "variable:a"););
    EXPECT_NO_THROW(pc_b = std::make_unique<EpicsChannel>(*test_pva_provider, "variable:b"););
    // EXPECT_NO_THROW(pc_sum->connect());
    // EXPECT_NO_THROW(pc_a->connect());
    // EXPECT_NO_THROW(pc_b->connect());
    EXPECT_NO_THROW(put_op_a = pc_a->put("value", "0"));
    EXPECT_NO_THROW(put_op_b = pc_b->put("value", "0"));
    WHILE(put_op_a->isDone(), false);
    EXPECT_EQ(put_op_a->getState().event, pvac::PutEvent::event_t::Success);
    WHILE(put_op_b->isDone(), false);
    EXPECT_EQ(put_op_b->getState().event, pvac::PutEvent::event_t::Success);
    EXPECT_EQ(retry_eq(*pc_sum, "value", 0, 500, 3), true);
    EXPECT_NO_THROW(put_op_a = pc_a->put("value", "5"));
    EXPECT_NO_THROW(put_op_b = pc_b->put("value", "5"));
    WHILE(put_op_a->isDone(), false);
    EXPECT_EQ(put_op_a->getState().event, pvac::PutEvent::event_t::Success);
    WHILE(put_op_b->isDone(), false);
    EXPECT_EQ(put_op_b->getState().event, pvac::PutEvent::event_t::Success);
    EXPECT_EQ(retry_eq(*pc_sum, "value", 10, 500, 3), true);
}

TEST(Epics, ChannelMonitor) {
     INIT_PVA_PROVIDER()
    EpicsChannelUPtr pc_a;
    ConstPutOperationUPtr put_op;
    epics::pvData::PVStructure::const_shared_pointer val;
    EXPECT_NO_THROW(pc_a = std::make_unique<EpicsChannel>(*test_pva_provider, "variable:a"););
    // EXPECT_NO_THROW(pc_a->connect());
    // enable monitor
    EXPECT_NO_THROW(put_op = pc_a->put("value", "0"););
    WHILE(put_op->isDone(), false);
    EXPECT_EQ(retry_eq(*pc_a, "value", 0, 500, 3), true);

    EXPECT_NO_THROW(pc_a->startMonitor(););
    MonitorEventVecShrdPtr fetched = pc_a->monitor();
    EXPECT_EQ(fetched->size(), 1);
    EXPECT_EQ(fetched->at(0)->type, MonitorType::Data);
    EXPECT_EQ(fetched->at(0)->channel_data.data->getSubField<epics::pvData::PVDouble>("value")->get(), 0);

    EXPECT_NO_THROW(put_op = pc_a->put("value", "1"););
    WHILE(put_op->isDone(), false);
    EXPECT_NO_THROW(put_op = pc_a->put("value", "2"););
    WHILE(put_op->isDone(), false);
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

TEST(Epics, EpicsServiceManagerWrongMonitoredDevices) {
    allEvent.clear();
    std::unique_ptr<EpicsServiceManager> monitor = std::make_unique<EpicsServiceManager>();
    EXPECT_NO_THROW(monitor->addHandler(std::bind(handler, std::placeholders::_1)););
    EXPECT_NO_THROW(monitor->addChannel("ca", "wrong::device"););
    auto vec = monitor->getMonitoredChannels();
    std::stringstream ss;
    for (auto& e: vec) {
        ss << e;
    }
    EXPECT_EQ(monitor->getChannelMonitoredSize(), 1) << "[" + ss.str() + "]";
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

TEST(Epics, EpicsServiceManagerGetPut) {
    allEvent.clear();
    ConstGetOperationUPtr sum_data;
    ConstPutOperationUPtr put_op_a;
    ConstPutOperationUPtr put_op_b;
    std::unique_ptr<EpicsServiceManager> manager = std::make_unique<EpicsServiceManager>();
    EXPECT_NO_THROW(put_op_a = manager->putChannelData("variable:a", "value", "1"););
    WHILE(put_op_a->isDone(), false);
    EXPECT_NO_THROW(put_op_b = manager->putChannelData("variable:b", "value", "2"););
    WHILE(put_op_b->isDone(), false);
    //give time to update
    std::this_thread::sleep_for(std::chrono::seconds(1));
    EXPECT_NO_THROW(sum_data = manager->getChannelData("variable:sum"););
    WHILE(sum_data->isDone(), false);
    EXPECT_EQ(sum_data->getChannelData()->data->getSubField<epics::pvData::PVDouble>("value")->get(), 3);
    manager.reset();
}

TEST(Epics, EpicsServiceManagerGetPutWaveForm) {
    allEvent.clear();
    ConstGetOperationUPtr sum_data;
    ConstPutOperationUPtr put_op_a;
    ConstPutOperationUPtr put_op_b;
    std::unique_ptr<EpicsServiceManager> manager = std::make_unique<EpicsServiceManager>();
    EXPECT_NO_THROW(put_op_a = manager->putChannelData("channel:waveform", "value", "1 2 3 4 5 6 7 8"););
    WHILE(put_op_a->isDone(), false);
    EXPECT_NO_THROW(sum_data = manager->getChannelData("channel:waveform"););
    WHILE(sum_data->isDone(), false);
    epics::pvData::PVScalarArray::const_shared_pointer arr_result;
    EXPECT_NO_THROW(arr_result = sum_data->getChannelData()->data->getSubField<epics::pvData::PVScalarArray>("value"));
    epics::pvData::shared_vector<const double> arr;
    arr_result->getAs<const double>(arr);
    EXPECT_EQ(arr.size(), 8);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
    EXPECT_EQ(arr[2], 3);
    EXPECT_EQ(arr[3], 4);
    EXPECT_EQ(arr[4], 5);
    EXPECT_EQ(arr[5], 6);
    EXPECT_EQ(arr[6], 7);
    EXPECT_EQ(arr[7], 8);
    manager.reset();
}