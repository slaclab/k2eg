#include <gtest/gtest.h>
#include <k2eg/common/utility.h>
#include <k2eg/controller/node/configuration/NodeConfiguration.h>
#include <k2eg/controller/node/worker/monitor/MonitorChecker.h>
#include <k2eg/service/data/DataStorage.h>

#include <filesystem>
#include <memory>

#include "k2eg/service/pubsub/IPublisher.h"

using std::make_shared;
using namespace k2eg::common;
using namespace k2eg::service::pubsub;
using namespace k2eg::controller::node::configuration;
using namespace k2eg::controller::node::worker::monitor;
using namespace k2eg::service::data;
using namespace k2eg::service::data::repository;
namespace fs = std::filesystem;

class DummyPublisher : public IPublisher {
 public:
  std::vector<PublishMessageSharedPtr> sent_messages;
  DummyPublisher() : IPublisher(std::make_unique<const PublisherConfiguration>(PublisherConfiguration{.server_address = "fake_address"})){};
  ~DummyPublisher() = default;
  void
  setAutoPoll(bool autopoll) {}
  int
  setCallBackForReqType(const std::string req_type, EventCallback eventCallback) {
    return 0;
  }
  int
  createQueue(const QueueDescription& queue) {
    return 0;
  }
  int
  deleteQueue(const std::string& queue_name) {
    return 0;
  }
  QueueMetadataUPtr
  getQueueMetadata(const std::string& queue_name) {
    return nullptr;
  }
  int
  flush(const int timeo) {
    return 0;
  }
  int
  pushMessage(PublishMessageUniquePtr message, const PublisherHeaders& header = PublisherHeaders()) {
    PublishMessageSharedPtr message_shrd_ptr = std::move(message);
    sent_messages.push_back(message_shrd_ptr);
    return 0;
  }
  int
  pushMessages(PublisherMessageVector& messages, const PublisherHeaders& header = PublisherHeaders()) {
    for (auto& uptr : messages) { sent_messages.push_back(std::move(uptr)); }
    return 0;
  }
  size_t
  getQueueMessageSize() {
    return sent_messages.size();
  }
};

TEST(NodeControllerMonitorChecker, StartMonitoringSingle) {
  int                          number_of_start_monitor = 0;
  std::shared_ptr<DataStorage> storage;
  EXPECT_NO_THROW(storage = std::make_shared<DataStorage>(fs::path(fs::current_path()) / "test.sqlite"););
  EXPECT_NO_THROW(toShared(storage->getChannelRepository())->removeAll(););
  auto                                    node_configuraiton = std::make_shared<NodeConfiguration>(storage);
  MonitorChecker                          checker(node_configuraiton);
  std::function<void(MonitorHandlerData)> checker_handler = [&number_of_start_monitor](MonitorHandlerData event_data) { 
    number_of_start_monitor++;
    ASSERT_EQ(event_data.action, MonitorHandlerAction::Start);
  };
  auto                                    event_token     = checker.addHandler(checker_handler);
  checker.storeMonitorData({ChannelMonitorType{.pv_name = "pv", .event_serialization = 0, .channel_protocol = "prot-a", .channel_destination = "dest-a"}});
  ASSERT_EQ(number_of_start_monitor, 1);
}

TEST(NodeControllerMonitorChecker, StartMonitoringSingleEventOnTwoSameMonitorRequestDifferentProtocol) {
  int                          number_of_start_monitor = 0;
  std::shared_ptr<DataStorage> storage;
  EXPECT_NO_THROW(storage = std::make_shared<DataStorage>(fs::path(fs::current_path()) / "test.sqlite"););
  EXPECT_NO_THROW(toShared(storage->getChannelRepository())->removeAll(););
  auto                                    node_configuraiton = std::make_shared<NodeConfiguration>(storage);
  MonitorChecker                          checker(node_configuraiton);
  std::function<void(MonitorHandlerData)> checker_handler = [&number_of_start_monitor](MonitorHandlerData event_data) { 
    number_of_start_monitor++;
    ASSERT_EQ(event_data.action, MonitorHandlerAction::Start);
  };
  auto                                    event_token     = checker.addHandler(checker_handler);
  checker.storeMonitorData({
    ChannelMonitorType{.pv_name = "pv", .event_serialization = 0, .channel_protocol = "prot-a", .channel_destination = "dest-a"},
    ChannelMonitorType{.pv_name = "pv", .event_serialization = 0, .channel_protocol = "prot-b", .channel_destination = "dest-a"}
    }
    );
  ASSERT_EQ(number_of_start_monitor, 1);
}

TEST(NodeControllerMonitorChecker, StartMonitoringDubleEventOnTwoSameMonitorRequestDifferentDestination) {
  int                          number_of_start_monitor = 0;
  std::shared_ptr<DataStorage> storage;
  EXPECT_NO_THROW(storage = std::make_shared<DataStorage>(fs::path(fs::current_path()) / "test.sqlite"););
  EXPECT_NO_THROW(toShared(storage->getChannelRepository())->removeAll(););
  auto                                    node_configuraiton = std::make_shared<NodeConfiguration>(storage);
  MonitorChecker                          checker(node_configuraiton);
  std::function<void(MonitorHandlerData)> checker_handler = [&number_of_start_monitor](MonitorHandlerData event_data) { 
    number_of_start_monitor++;
    ASSERT_EQ(event_data.action, MonitorHandlerAction::Start);
  };
  auto                                    event_token     = checker.addHandler(checker_handler);
  checker.storeMonitorData({
    ChannelMonitorType{.pv_name = "pv", .event_serialization = 0, .channel_protocol = "prot-a", .channel_destination = "dest-a"},
    ChannelMonitorType{.pv_name = "pv", .event_serialization = 0, .channel_protocol = "prot-b", .channel_destination = "dest-b"}
    }
    );
  ASSERT_EQ(number_of_start_monitor, 2);
}