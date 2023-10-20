#include <gtest/gtest.h>
#include <k2eg/common/utility.h>
#include <k2eg/controller/node/configuration/NodeConfiguration.h>
#include <k2eg/controller/node/worker/monitor/MonitorChecker.h>
#include <k2eg/service/data/DataStorage.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "NodeControllerCommon.h"
#include "k2eg/common/ProgramOptions.h"
#include "k2eg/service/ServiceResolver.h"
#include "k2eg/service/log/ILogger.h"
#include "k2eg/service/log/impl/BoostLogger.h"
#include "k2eg/service/pubsub/IPublisher.h"

using std::make_shared;
using namespace k2eg::common;
using namespace k2eg::service;
using namespace k2eg::service::log;
using namespace k2eg::service::log::impl;
using namespace k2eg::service::pubsub;
using namespace k2eg::controller::node::configuration;
using namespace k2eg::controller::node::worker::monitor;
using namespace k2eg::service::data;
using namespace k2eg::service::data::repository;
namespace fs = std::filesystem;

MonitorCheckerUPtr
initChecker(IPublisherShrdPtr pub, bool clear_data = true, bool enable_debug_log = false) {
  int         argc    = 1;
  const char* argv[1] = {"epics-k2eg-test"};
  clearenv();
  if (enable_debug_log) {
    setenv("EPICS_k2eg_log-on-console", "true", 1);
    setenv("EPICS_k2eg_log-level", "trace", 1);
  } else {
    setenv("EPICS_k2eg_log-on-console", "false", 1);
  }
  std::unique_ptr<ProgramOptions> opt = std::make_unique<ProgramOptions>();
  opt->parse(argc, argv);
  ServiceResolver<ILogger>::registerService(std::make_shared<BoostLogger>(opt->getloggerConfiguration()));
  ServiceResolver<IPublisher>::registerService(pub);
  DataStorageShrdPtr storage = std::make_shared<DataStorage>(fs::path(fs::current_path()) / "test.sqlite");
  if (clear_data) { toShared(storage->getChannelRepository())->removeAll(); }
  auto node_configuraiton = std::make_shared<NodeConfiguration>(storage);
  return MakeMonitorCheckerUPtr(opt->getNodeControllerConfiguration()->monitor_command_configuration.monitor_checker_configuration, node_configuraiton);
}

void checkerAutomaticManagementForStop(MonitorChecker& checker) {
  if(!checker.scanForMonitorToStop()) {
    checker.resetMonitorToProcess();
    checker.scanForMonitorToStop();
  }
}

void
deinitChecker() {
  EXPECT_NO_THROW(ServiceResolver<IPublisher>::resolve().reset(););
  ;
  EXPECT_NO_THROW(ServiceResolver<ILogger>::resolve().reset(););
}

TEST(NodeControllerMonitorChecker, StartMonitoringSingle) {
  int                                     number_of_start_monitor = 0;
  std::shared_ptr<IPublisher>             pub                     = std::make_shared<ControllerConsumerDummyPublisher>();
  auto                                    checker                 = initChecker(pub, true, false);
  std::function<void(MonitorHandlerData)> checker_handler         = [&number_of_start_monitor](MonitorHandlerData event_data) {
    number_of_start_monitor++;
    ASSERT_EQ(event_data.action, MonitorHandlerAction::Start);
  };
  auto event_token = checker->addHandler(checker_handler);
  checker->storeMonitorData({ChannelMonitorType{.pv_name = "pv", .event_serialization = 0, .channel_protocol = "prot-a", .channel_destination = "dest-a"}});
  ASSERT_EQ(number_of_start_monitor, 1);
  deinitChecker();
}

TEST(NodeControllerMonitorChecker, StartMonitoringSingleEventOnTwoSameMonitorRequestDifferentProtocol) {
  int                                     number_of_start_monitor = 0;
  std::shared_ptr<IPublisher>             pub                     = std::make_shared<ControllerConsumerDummyPublisher>();
  auto                                    checker                 = initChecker(pub, true, false);
  std::function<void(MonitorHandlerData)> checker_handler         = [&number_of_start_monitor](MonitorHandlerData event_data) {
    number_of_start_monitor++;
    ASSERT_EQ(event_data.action, MonitorHandlerAction::Start);
  };
  auto event_token = checker->addHandler(checker_handler);
  checker->storeMonitorData({ChannelMonitorType{.pv_name = "pv", .event_serialization = 0, .channel_protocol = "prot-a", .channel_destination = "dest-a"},
                             ChannelMonitorType{.pv_name = "pv", .event_serialization = 0, .channel_protocol = "prot-b", .channel_destination = "dest-a"}});
  ASSERT_EQ(number_of_start_monitor, 1);
  deinitChecker();
}

TEST(NodeControllerMonitorChecker, StartMonitoringDubleEventOnTwoSameMonitorRequestDifferentDestination) {
  int                                     number_of_start_monitor = 0;
  std::shared_ptr<IPublisher>             pub                     = std::make_shared<ControllerConsumerDummyPublisher>();
  auto                                    checker                 = initChecker(pub, true, false);
  std::function<void(MonitorHandlerData)> checker_handler         = [&number_of_start_monitor](MonitorHandlerData event_data) {
    number_of_start_monitor++;
    ASSERT_EQ(event_data.action, MonitorHandlerAction::Start);
  };
  auto event_token = checker->addHandler(checker_handler);
  checker->storeMonitorData({ChannelMonitorType{.pv_name = "pv", .event_serialization = 0, .channel_protocol = "prot-a", .channel_destination = "dest-a"},
                             ChannelMonitorType{.pv_name = "pv", .event_serialization = 0, .channel_protocol = "prot-b", .channel_destination = "dest-b"}});
  ASSERT_EQ(number_of_start_monitor, 2);
  deinitChecker();
}

TEST(NodeControllerMonitorChecker, ScanForMonitorToStop) {
  int                                     number_of_start_monitor = 0;
  int                                     number_of_stop_monitor  = 0;
  std::shared_ptr<IPublisher>             pub                     = std::make_shared<ControllerConsumerDummyPublisher>();
  auto                                    checker                 = initChecker(pub, true, false);
  std::function<void(MonitorHandlerData)> checker_handler         = [&number_of_start_monitor, &number_of_stop_monitor](MonitorHandlerData event_data) {
    switch (event_data.action) {
      case MonitorHandlerAction::Start: {number_of_start_monitor++; break;}
      case MonitorHandlerAction::Stop: {number_of_stop_monitor++; break;}
    }
  };
  auto event_token = checker->addHandler(checker_handler);
  checker->storeMonitorData({ChannelMonitorType{.pv_name = "pv", .event_serialization = 0, .channel_protocol = "prot-a", .channel_destination = "dest-a"}});
  ASSERT_EQ(number_of_start_monitor, 1);
  // add a simulated consumer
  dynamic_cast<ControllerConsumerDummyPublisher*>(pub.get())->setConsumerNumber(1);
  // execute checking
  checkerAutomaticManagementForStop(*checker);
  // this time no stop signal received
  ASSERT_EQ(number_of_stop_monitor, 0);

  dynamic_cast<ControllerConsumerDummyPublisher*>(pub.get())->setConsumerNumber(0);
  // set high timeout for simulate that is not the time to delete
  checker->setPurgeTimeout(3600);
  checkerAutomaticManagementForStop(*checker);
  // st timeout low for let chek ca trigger the delete of the queue
  sleep(2);
  checker->setPurgeTimeout(1);
  checkerAutomaticManagementForStop(*checker);
  // now we need to be called for the delete of the monitor
  ASSERT_EQ(number_of_stop_monitor, 1);
  deinitChecker();
}