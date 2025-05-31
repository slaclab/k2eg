#include "epics.h"

#include <gtest/gtest.h>
#include <k2eg/service/ServiceResolver.h>
#include <k2eg/service/epics/EpicsChannel.h>
#include <k2eg/service/epics/EpicsServiceManager.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/log/impl/BoostLogger.h>
#include <k2eg/service/metric/IMetricService.h>
#include <k2eg/service/metric/impl/DummyMetricService.h>
#include <k2eg/service/scheduler/Scheduler.h>
#include <unistd.h>

#include <chrono>
#include <latch>
#include <thread>

#include "k2eg/service/epics/EpicsGetOperation.h"
#include "k2eg/service/epics/EpicsMonitorOperation.h"
#include "k2eg/service/epics/EpicsPutOperation.h"
#include "pvData.h"

using namespace k2eg::service::epics_impl;

TEST(EpicsChannel, ChannelFault)
{
    INIT_CA_PROVIDER()
    EpicsChannelUPtr pc;
    EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_ca_provider, "bacd_pv_name"));
}

TEST(EpicsChannel, ChannelPVAGetOpOk)
{
    INIT_PVA_PROVIDER()
    EpicsChannelUPtr                                 pc;
    ConstGetOperationUPtr                            get_op;
    epics::pvData::PVStructure::const_shared_pointer val;
    EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_pva_provider, "variable:sum"););
    EXPECT_NO_THROW(get_op = pc->get(););
    WHILE_OP(get_op, false);
    EXPECT_EQ(get_op->getState().event, pvac::GetEvent::Success);
}

TEST(EpicsChannel, ChannelCAGetOpOk)
{
    INIT_CA_PROVIDER()
    EpicsChannelUPtr                                 pc;
    ConstGetOperationUPtr                            get_op;
    epics::pvData::PVStructure::const_shared_pointer val;
    EXPECT_NO_THROW(pc = std::make_unique<EpicsChannel>(*test_ca_provider, "variable:sum"););
    EXPECT_NO_THROW(get_op = pc->get(););
    WHILE_OP(get_op, false);
    EXPECT_EQ(get_op->getState().event, pvac::GetEvent::Success);
}

bool retry_eq(const ConstGetOperationUPtr& get_op, const std::string& name, double value, int mseconds, int retry_times)
{
    for (int times = retry_times; times != 0; times--)
    {
        if (get_op->isDone())
        {
            auto dval = get_op->getChannelData()->data->getSubField<epics::pvData::PVDouble>(name)->get();
            return dval == value;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(mseconds));
    }
    return false;
}

TEST(EpicsChannel, ChannelPutValue)
{
    INIT_PVA_PROVIDER()
    ConstPutOperationUPtr                            put_op_a;
    ConstPutOperationUPtr                            put_op_b;
    EpicsChannelUPtr                                 pc_sum;
    EpicsChannelUPtr                                 pc_a;
    EpicsChannelUPtr                                 pc_b;
    epics::pvData::PVStructure::const_shared_pointer val;
    EXPECT_NO_THROW(pc_sum = std::make_unique<EpicsChannel>(*test_pva_provider, "variable:sum"););
    EXPECT_NO_THROW(pc_a = std::make_unique<EpicsChannel>(*test_pva_provider, "variable:a"););
    EXPECT_NO_THROW(pc_b = std::make_unique<EpicsChannel>(*test_pva_provider, "variable:b"););
    EXPECT_NO_THROW(put_op_a = pc_a->put("value", "0"));
    WHILE_OP(put_op_a, false);
    EXPECT_EQ(put_op_a->getState().event, pvac::PutEvent::event_t::Success);
    EXPECT_NO_THROW(put_op_b = pc_b->put("value", "0"));
    WHILE_OP(put_op_b, false);
    EXPECT_EQ(put_op_b->getState().event, pvac::PutEvent::event_t::Success);
    // give time to update
    sleep(2);
    EXPECT_EQ(retry_eq(pc_sum->get(), "value", 0, 1000, 10), true);
    EXPECT_NO_THROW(put_op_a = pc_a->put("value", "5"));
    WHILE_OP(put_op_a, false);
    EXPECT_EQ(put_op_a->getState().event, pvac::PutEvent::event_t::Success);
    EXPECT_NO_THROW(put_op_b = pc_b->put("value", "5"));
    WHILE_OP(put_op_b, false);
    EXPECT_EQ(put_op_b->getState().event, pvac::PutEvent::event_t::Success);
    // give time to update
    sleep(2);
    EXPECT_EQ(retry_eq(pc_sum->get(), "value", 10, 1000, 10), true);
}

TEST(EpicsChannel, ChannelMonitor)
{
    INIT_PVA_PROVIDER()
    EpicsChannelUPtr                                 pc_a;
    ConstPutOperationUPtr                            put_op;
    ConstMonitorOperationShrdPtr                     monitor_op;
    epics::pvData::PVStructure::const_shared_pointer val;
    EXPECT_NO_THROW(pc_a = std::make_unique<EpicsChannel>(*test_pva_provider, "variable:a"););
    // enable monitor
    EXPECT_NO_THROW(put_op = pc_a->put("value", "0"););
    WHILE_OP(put_op, false);
    EXPECT_EQ(retry_eq(pc_a->get(), "value", 0, 500, 3), true);

    EXPECT_NO_THROW(monitor_op = pc_a->monitor(););
    WHILE_MONITOR(monitor_op, !monitor_op->hasData());
    auto fetched = monitor_op->getEventData();
    EXPECT_EQ(fetched->event_data->size(), 1);
    EXPECT_EQ(fetched->event_data->at(0)->type, EventType::Data);
    EXPECT_EQ(fetched->event_data->at(0)->channel_data.data->getSubField<epics::pvData::PVDouble>("value")->get(), 0);

    EXPECT_NO_THROW(put_op = pc_a->put("value", "1"););
    WHILE_OP(put_op, false);
    EXPECT_NO_THROW(put_op = pc_a->put("value", "2"););
    WHILE_OP(put_op, false);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    WHILE_MONITOR(monitor_op, !monitor_op->hasData());
    fetched = monitor_op->getEventData();
    EXPECT_EQ(fetched->event_data->size(), 2);
    EXPECT_EQ(fetched->event_data->at(0)->type, EventType::Data);
    EXPECT_EQ(fetched->event_data->at(0)->channel_data.data->getSubField<epics::pvData::PVDouble>("value")->get(), 1);
    EXPECT_EQ(fetched->event_data->at(1)->type, EventType::Data);
    EXPECT_EQ(fetched->event_data->at(1)->channel_data.data->getSubField<epics::pvData::PVDouble>("value")->get(), 2);
    EXPECT_NO_THROW(monitor_op.reset(););
}

TEST(EpicsChannel, ChannelMonitorWrongPv)
{
    INIT_PVA_PROVIDER()
    EpicsChannelUPtr                                 pc_a;
    ConstPutOperationUPtr                            put_op;
    ConstMonitorOperationShrdPtr                     monitor_op;
    epics::pvData::PVStructure::const_shared_pointer val;
    EXPECT_NO_THROW(pc_a = std::make_unique<EpicsChannel>(*test_pva_provider, "pad:pv"););
    // enable monitor
    EXPECT_NO_THROW(monitor_op = pc_a->monitor(););
    WHILE_MONITOR(monitor_op, !monitor_op->hasEvents());
    auto fetched = monitor_op->getEventData();
    EXPECT_EQ(fetched->event_fail->size(), 1);

    EXPECT_NO_THROW(monitor_op.reset(););
}

TEST(EpicsChannel, ChannelMonitorForceUpdateStalePv)
{
    INIT_PVA_PROVIDER()
    EpicsChannelUPtr                                 pc_a;
    ConstPutOperationUPtr                            put_op;
    ConstMonitorOperationShrdPtr                     monitor_op;
    epics::pvData::PVStructure::const_shared_pointer val;
    EXPECT_NO_THROW(pc_a = std::make_unique<EpicsChannel>(*test_pva_provider, "variable:a"););
    // enable monitor
    EXPECT_NO_THROW(monitor_op = pc_a->monitor(););
    WHILE_MONITOR(monitor_op, !monitor_op->hasData());
    auto fetched = monitor_op->getEventData();
    EXPECT_EQ(fetched->event_data->size(), 1);
    EXPECT_EQ(fetched->event_data->at(0)->type, EventType::Data);
    // force the update
    monitor_op->forceUpdate();
    WHILE_MONITOR(monitor_op, !monitor_op->hasData());
    fetched = monitor_op->getEventData();
    EXPECT_EQ(fetched->event_data->size(), 1);
    EXPECT_EQ(fetched->event_data->at(0)->type, EventType::Data);
    EXPECT_NO_THROW(monitor_op.reset(););
}

TEST(EpicsChannel, ChannelMonitorCombinedRequestCA)
{
    INIT_CA_PROVIDER()
    EpicsChannelUPtr                                 pc_a;
    ConstPutOperationUPtr                            put_op;
    ConstMonitorOperationShrdPtr                     monitor_op;
    epics::pvData::PVStructure::const_shared_pointer val;
    EXPECT_NO_THROW(pc_a = std::make_unique<EpicsChannel>(*test_ca_provider, "variable:a"););
    // enable monitor
    EXPECT_NO_THROW(put_op = pc_a->put("value", "0"););
    WHILE_OP(put_op, false);
    EXPECT_EQ(retry_eq(pc_a->get(), "value", 0, 500, 3), true);

    EXPECT_NO_THROW(monitor_op = pc_a->monitor(););
    WHILE_MONITOR(monitor_op, !monitor_op->hasData());
    auto fetched = monitor_op->getEventData();
    EXPECT_EQ(fetched->event_data->size(), 1);
    EXPECT_EQ(fetched->event_data->at(0)->type, EventType::Data);
    EXPECT_EQ(fetched->event_data->at(0)->channel_data.data->getSubField<epics::pvData::PVDouble>("value")->get(), 0);

    EXPECT_NO_THROW(put_op = pc_a->put("value", "1"););
    WHILE_OP(put_op, false);
    EXPECT_NO_THROW(put_op = pc_a->put("value", "2"););
    WHILE_OP(put_op, false);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    WHILE_MONITOR(monitor_op, !monitor_op->hasData());
    fetched = monitor_op->getEventData();
    EXPECT_EQ(fetched->event_data->size(), 2);
    EXPECT_EQ(fetched->event_data->at(0)->type, EventType::Data);
    EXPECT_EQ(fetched->event_data->at(0)->channel_data.data->getSubField<epics::pvData::PVDouble>("value")->get(), 1);
    EXPECT_EQ(fetched->event_data->at(1)->type, EventType::Data);
    EXPECT_EQ(fetched->event_data->at(1)->channel_data.data->getSubField<epics::pvData::PVDouble>("value")->get(), 2);
    EXPECT_NO_THROW(monitor_op.reset(););
}

TEST(EpicsChannel, ChannelMonitorCombinedRequestCAForceUpdateStalePv)
{
    INIT_CA_PROVIDER()
    EpicsChannelUPtr                                 pc_a;
    ConstPutOperationUPtr                            put_op;
    ConstMonitorOperationShrdPtr                     monitor_op;
    epics::pvData::PVStructure::const_shared_pointer val;
    EXPECT_NO_THROW(pc_a = std::make_unique<EpicsChannel>(*test_ca_provider, "variable:a"););

    EXPECT_NO_THROW(monitor_op = pc_a->monitor(););
    WHILE_MONITOR(monitor_op, !monitor_op->hasData());
    auto fetched = monitor_op->getEventData();
    EXPECT_EQ(fetched->event_data->size(), 1);
    EXPECT_EQ(fetched->event_data->at(0)->type, EventType::Data);

    monitor_op->forceUpdate();
    WHILE_MONITOR(monitor_op, !monitor_op->hasData());
    fetched = monitor_op->getEventData();
    EXPECT_EQ(fetched->event_data->size(), 1);
    EXPECT_EQ(fetched->event_data->at(0)->type, EventType::Data);
    EXPECT_NO_THROW(monitor_op.reset(););
}

TEST(EpicsChannel, ChannelCAMonitor)
{
    INIT_CA_PROVIDER()
    EpicsChannelUPtr                                 pc_a;
    ConstPutOperationUPtr                            put_op;
    ConstMonitorOperationShrdPtr                     monitor_op;
    epics::pvData::PVStructure::const_shared_pointer val;
    EXPECT_NO_THROW(pc_a = std::make_unique<EpicsChannel>(*test_ca_provider, "variable:a"););
    // enable monitor
    EXPECT_NO_THROW(put_op = pc_a->put("value", "0"););
    WHILE_OP(put_op, false);
    EXPECT_EQ(retry_eq(pc_a->get(), "value", 0, 500, 3), true);

    EXPECT_NO_THROW(monitor_op = pc_a->monitor(););
    WHILE_MONITOR(monitor_op, !monitor_op->hasData());
    auto fetched = monitor_op->getEventData();
    EXPECT_EQ(fetched->event_data->size(), 1);
    EXPECT_EQ(fetched->event_data->at(0)->type, EventType::Data);
    EXPECT_EQ(fetched->event_data->at(0)->channel_data.data->getSubField<epics::pvData::PVDouble>("value")->get(), 0);
    EXPECT_NO_THROW(put_op = pc_a->put("value", "1"););
    WHILE_OP(put_op, false);
    EXPECT_NO_THROW(put_op = pc_a->put("value", "2"););
    WHILE_OP(put_op, false);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    WHILE_MONITOR(monitor_op, !monitor_op->hasData());
    fetched = monitor_op->getEventData();
    EXPECT_EQ(fetched->event_data->size(), 2);
    EXPECT_EQ(fetched->event_data->at(0)->type, EventType::Data);
    EXPECT_EQ(fetched->event_data->at(0)->channel_data.data->getSubField<epics::pvData::PVDouble>("value")->get(), 1);
    EXPECT_EQ(fetched->event_data->at(1)->type, EventType::Data);
    EXPECT_EQ(fetched->event_data->at(1)->channel_data.data->getSubField<epics::pvData::PVDouble>("value")->get(), 2);
    EXPECT_NO_THROW(monitor_op.reset(););
}

struct HandlerClass
{
    std::latch           work_done;
    EventReceivedShrdPtr event_received = std::make_shared<EventReceived>();

    HandlerClass(int event_size) : work_done(event_size) {}

    void handler(EpicsServiceManagerHandlerParamterType event)
    {
        event_received = event;
        work_done.count_down();
    }
};

// setup the test environment
using namespace k2eg::service;
using namespace k2eg::service::log;
using namespace k2eg::service::log::impl;
using namespace k2eg::service::epics_impl;
using namespace k2eg::service::scheduler;
using namespace k2eg::service::metric;
using namespace k2eg::service::metric::impl;

class Epics : public ::testing::Test
{
protected:
    Epics() {}

    virtual ~Epics() {}

    virtual void SetUp()
    {
        ServiceResolver<ILogger>::registerService(std::make_shared<BoostLogger>(MakeLogConfigurationUPtr(LogConfiguration{})));
        ServiceResolver<Scheduler>::registerService(std::make_shared<Scheduler>(MakeSchedulerConfigurationUPtr(SchedulerConfiguration{})));
        ServiceResolver<IMetricService>::registerService(std::make_shared<DummyMetricService>(MakeMetricConfigurationUPtr(MetricConfiguration{})));
    }

    virtual void TearDown()
    {
        ServiceResolver<IMetricService>::reset();
        ServiceResolver<Scheduler>::reset();
        ServiceResolver<ILogger>::reset();
    }
};

TEST_F(Epics, EpicsServiceManagerPVSanitizationfailWithNoProtocol)
{
    PVUPtr                               pv_desc;
    std::unique_ptr<EpicsServiceManager> manager = std::make_unique<EpicsServiceManager>();
    EXPECT_NO_THROW(pv_desc = manager->sanitizePVName("VGXX-L3B-1602-PLOG"););
    EXPECT_EQ(pv_desc, nullptr);
    manager.reset();
}

TEST_F(Epics, EpicsServiceManagerPVSanitizationUppercaseAndDash)
{
    PVUPtr                               pv_desc;
    std::unique_ptr<EpicsServiceManager> manager = std::make_unique<EpicsServiceManager>();
    EXPECT_NO_THROW(pv_desc = manager->sanitizePVName("pva://VGXX-L3B-1602-PLOG"););
    EXPECT_STREQ(pv_desc->protocol.c_str(), "pva");
    EXPECT_STREQ(pv_desc->name.c_str(), "VGXX-L3B-1602-PLOG");
    EXPECT_STREQ(pv_desc->field.c_str(), "value");
    manager.reset();
}

TEST_F(Epics, EpicsServiceManagerPVSanitization)
{
    PVUPtr                               pv_desc;
    std::unique_ptr<EpicsServiceManager> manager = std::make_unique<EpicsServiceManager>();
    EXPECT_NO_THROW(pv_desc = manager->sanitizePVName("ca://variable:a.HIHI"););
    EXPECT_STREQ(pv_desc->protocol.c_str(), "ca");
    EXPECT_STREQ(pv_desc->name.c_str(), "variable:a");
    EXPECT_STREQ(pv_desc->field.c_str(), "HIHI");
    manager.reset();
}

TEST_F(Epics, EpicsServiceManagerPVSanitizationWithNoName)
{
    PVUPtr                               pv_desc;
    std::unique_ptr<EpicsServiceManager> manager = std::make_unique<EpicsServiceManager>();
    EXPECT_NO_THROW(pv_desc = manager->sanitizePVName(""););
    EXPECT_EQ(pv_desc, nullptr);
    manager.reset();
}

TEST_F(Epics, EpicsServiceManagerPVSanitizationOkWithMultipleLevelStructure)
{
    PVUPtr                               pv_desc;
    std::unique_ptr<EpicsServiceManager> manager = std::make_unique<EpicsServiceManager>();
    EXPECT_NO_THROW(pv_desc = manager->sanitizePVName("pva://variable:a.root.field"););
    EXPECT_STREQ(pv_desc->protocol.c_str(), "pva");
    EXPECT_STREQ(pv_desc->name.c_str(), "variable:a");
    EXPECT_STREQ(pv_desc->field.c_str(), "root.field");
    manager.reset();
}

TEST_F(Epics, EpicsServiceManagerMonitorOk)
{
    HandlerClass                         handler(1);
    k2eg::common::BroadcastToken         handler_tok;
    std::unique_ptr<EpicsServiceManager> monitor = std::make_unique<EpicsServiceManager>();
    EXPECT_NO_THROW(handler_tok = monitor->addHandler(std::bind(&HandlerClass::handler, &handler, std::placeholders::_1)););
    EXPECT_NO_THROW(monitor->addChannel("pva://channel:ramp:ramp"););
    while (handler.event_received->event_data->size() == 0)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    EXPECT_EQ(handler.event_received->event_data->size() > 0, true);
    monitor.reset();
}

TEST_F(Epics, EpicsServiceManagerMonitorMultipleInstanceOk)
{
    HandlerClass                         handler(1);
    k2eg::common::BroadcastToken         handler_tok;
    std::unique_ptr<EpicsServiceManager> manager = std::make_unique<EpicsServiceManager>();
    EXPECT_NO_THROW(handler_tok = manager->addHandler(std::bind(&HandlerClass::handler, &handler, std::placeholders::_1)););
    EXPECT_NO_THROW(manager->addChannel("pva://channel:ramp:ramp"););
    EXPECT_NO_THROW(manager->addChannel("pva://channel:ramp:ramp"););
    EXPECT_NO_THROW(manager->removeChannel("pva://channel:ramp:ramp"););
    while (handler.event_received->event_data->size() == 0)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    EXPECT_NO_THROW(manager->removeChannel("pva://channel:ramp:ramp"););
    EXPECT_EQ(handler.event_received->event_data->size() > 0, true);
    sleep(1);
    EXPECT_EQ(manager->getChannelMonitoredSize() == 0, true);
    manager.reset();
}

TEST_F(Epics, EpicsServiceManagerMonitorStalePVOk)
{
    HandlerClass                         handler(2);
    k2eg::common::BroadcastToken         handler_tok;
    std::unique_ptr<EpicsServiceManager> monitor = std::make_unique<EpicsServiceManager>();
    EXPECT_NO_THROW(handler_tok = monitor->addHandler(std::bind(&HandlerClass::handler, &handler, std::placeholders::_1)););
    EXPECT_NO_THROW(monitor->addChannel("ca://variable:a"););
    sleep(2);
    EXPECT_NO_THROW(monitor->forceMonitorChannelUpdate("ca://variable:a"););
    handler.work_done.wait();
    EXPECT_EQ(handler.event_received->event_data->size() > 0, true);
    monitor.reset();
}

TEST_F(Epics, EpicsServiceManagerWrongMonitoredDevices)
{
    HandlerClass                         handler(0);
    k2eg::common::BroadcastToken         handler_tok;
    std::unique_ptr<EpicsServiceManager> monitor = std::make_unique<EpicsServiceManager>();
    EXPECT_NO_THROW(handler_tok = monitor->addHandler(std::bind(&HandlerClass::handler, &handler, std::placeholders::_1)););
    EXPECT_NO_THROW(monitor->addChannel("pva://wrong::device"););
    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_EQ(handler.event_received->event_timeout->size() == 0, true);
    monitor.reset();
}

TEST_F(Epics, EpicsServiceManagerUnreachableMonitoredDevices)
{
    HandlerClass                         handler(0);
    k2eg::common::BroadcastToken         handler_tok;
    std::unique_ptr<EpicsServiceManager> monitor = std::make_unique<EpicsServiceManager>();
    EXPECT_NO_THROW(handler_tok = monitor->addHandler(std::bind(&HandlerClass::handler, &handler, std::placeholders::_1)););
    EXPECT_NO_THROW(monitor->addChannel("pva://wrong:device"););
    while (handler.event_received->event_fail->size() == 0)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    EXPECT_EQ(handler.event_received->event_fail->size() != 0, true);
    monitor.reset();
}

TEST_F(Epics, EpicsServiceManagerAddRemove)
{
    std::unique_ptr<EpicsServiceManager> monitor = std::make_unique<EpicsServiceManager>();
    EXPECT_NO_THROW(monitor->addChannel("pva://wrong::device"););
    EXPECT_NO_THROW(monitor->removeChannel("wrong::device"););
    EXPECT_EQ(monitor->getChannelMonitoredSize(), 0);
    monitor.reset();
}

TEST_F(Epics, EpicsServiceManagerRemoveHandler)
{
    k2eg::common::BroadcastToken         handler_tok;
    HandlerClass                         handler(1);
    std::unique_ptr<EpicsServiceManager> monitor = std::make_unique<EpicsServiceManager>();
    EXPECT_NO_THROW(handler_tok = monitor->addHandler(std::bind(&HandlerClass::handler, &handler, std::placeholders::_1)););
    EXPECT_EQ(monitor->getHandlerSize(), 1);
    handler_tok.reset();                     // this should invalidate the handler within the manager
    EXPECT_EQ(monitor->getHandlerSize(), 0); // this should be 0 because of handler_tok.reset()
    monitor.reset();
}

TEST_F(Epics, EpicsServiceManagerGetPut)
{
    ConstGetOperationUPtr                sum_data;
    ConstPutOperationUPtr                put_op_a;
    ConstPutOperationUPtr                put_op_b;
    std::unique_ptr<EpicsServiceManager> manager = std::make_unique<EpicsServiceManager>();
    EXPECT_NO_THROW(put_op_a = manager->putChannelData("pva://variable:a", "1"););
    WHILE_OP(put_op_a, false);
    EXPECT_NO_THROW(put_op_b = manager->putChannelData("pva://variable:b", "2"););
    WHILE_OP(put_op_b, false);
    // give time to update
    std::this_thread::sleep_for(std::chrono::seconds(1));
    EXPECT_NO_THROW(sum_data = manager->getChannelData("pva://variable:sum"););
    WHILE_OP(sum_data, false);
    EXPECT_EQ(sum_data->getChannelData()->data->getSubField<epics::pvData::PVDouble>("value")->get(), 3);
    manager.reset();
}

TEST_F(Epics, EpicsServiceManagerGetPutWaveForm)
{
    ConstGetOperationUPtr                sum_data;
    ConstPutOperationUPtr                put_op_a;
    ConstPutOperationUPtr                put_op_b;
    std::unique_ptr<EpicsServiceManager> manager = std::make_unique<EpicsServiceManager>();
    EXPECT_NO_THROW(put_op_a = manager->putChannelData("pva://channel:waveform", "1 2 3 4 5 6 7 8"););
    WHILE_OP(put_op_a, false);
    EXPECT_NO_THROW(sum_data = manager->getChannelData("pva://channel:waveform"););
    WHILE_OP(sum_data, false);
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

TEST_F(Epics, EpicsServiceManagerPutWrongField)
{
    ConstGetOperationUPtr                sum_data;
    ConstPutOperationUPtr                put_op_a;
    ConstPutOperationUPtr                put_op_b;
    std::unique_ptr<EpicsServiceManager> manager = std::make_unique<EpicsServiceManager>();
    EXPECT_NO_THROW(put_op_a = manager->putChannelData("pva://variable:a.HIHI", "100"););
    WHILE_OP(put_op_a, false);
    EXPECT_EQ(put_op_a->getState().event, pvac::PutEvent::Fail);
    manager.reset();
}

TEST_F(Epics, EpicsServiceManagerPutOtherField)
{
    ConstGetOperationUPtr                sum_data;
    ConstPutOperationUPtr                put_op_a;
    ConstPutOperationUPtr                put_op_b;
    std::unique_ptr<EpicsServiceManager> manager = std::make_unique<EpicsServiceManager>();
    // EXPECT_NO_THROW(put_op_a = manager->putChannelData("variable:a.valueAlarm.highWarningLimit", "200"););
    // WHILE(put_op_a, false);
    // EXPECT_EQ(put_op_a->getState().event, pvac::PutEvent::Success);
    // EXPECT_NO_THROW(sum_data = manager->getChannelData("variable:a"););
    // WHILE(sum_data, false);
    // epics::pvData::PVScalar::const_shared_pointer scalar_result;
    // EXPECT_NO_THROW(scalar_result =
    // sum_data->getChannelData()->data->getSubField<epics::pvData::PVScalar>("valueAlarm.highWarningLimit")); int
    // hihi_result = scalar_result->getAs<epics::pvData::uint32>(); EXPECT_EQ(hihi_result, 200);
    manager.reset();
}