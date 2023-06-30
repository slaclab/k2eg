#include <gtest/gtest.h>
#include <k2eg/common/utility.h>
#include <k2eg/service/data/DataStorage.h>

#include <filesystem>
#include <latch>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using namespace k2eg::common;
using namespace k2eg::controller::command::cmd;
using namespace k2eg::service::data;
using namespace k2eg::service::data::repository;

TEST(DataStorage, Default) {
  std::unique_ptr<DataStorage> storage;
  EXPECT_NO_THROW(storage = std::make_unique<DataStorage>(fs::path(fs::current_path()) / "test.sqlite"););
  EXPECT_NO_THROW(toShared(storage->getPVRepository())->removeAll(););
  EXPECT_NO_THROW(toShared(storage->getPVRepository())
                      ->insert({.pv_name             = "channel::a",
                                .event_serialization = static_cast<uint8_t>(SerializationType::JSON),
                                .pv_protocol         = "pva",
                                .pv_destination      = "dest"}););
  auto found_pv = toShared(storage->getPVRepository())->getPVMonitor("channel::a", "dest");

  EXPECT_EQ(found_pv.has_value(), true);
  EXPECT_STREQ(found_pv->get()->pv_name.c_str(), "channel::a");
  EXPECT_EQ(found_pv->get()->event_serialization, static_cast<uint8_t>(SerializationType::JSON));
  EXPECT_STREQ(found_pv->get()->pv_protocol.c_str(), "pva");
  EXPECT_STREQ(found_pv->get()->pv_destination.c_str(), "dest");
}

TEST(DataStorage, SimulateMultipleMonitorSamePVTopic) {
  std::unique_ptr<DataStorage> storage;
  EXPECT_NO_THROW(storage = std::make_unique<DataStorage>(fs::path(fs::current_path()) / "test.sqlite"););
  EXPECT_NO_THROW(toShared(storage->getPVRepository())->removeAll(););
  EXPECT_NO_THROW(toShared(storage->getPVRepository())
                      ->insert({.pv_name             = "channel::a",
                                .event_serialization = static_cast<uint8_t>(SerializationType::JSON),
                                .pv_protocol         = "pva",
                                .pv_destination      = "dest"}););
  EXPECT_NO_THROW(toShared(storage->getPVRepository())
                      ->insert({.pv_name             = "channel::a",
                                .event_serialization = static_cast<uint8_t>(SerializationType::JSON),
                                .pv_protocol         = "pva",
                                .pv_destination      = "dest"}););
  auto found_pv = toShared(storage->getPVRepository())->getPVMonitor("channel::a", "dest");

  EXPECT_EQ(found_pv.has_value(), true);
  EXPECT_STREQ(found_pv->get()->pv_name.c_str(), "channel::a");
  EXPECT_EQ(found_pv->get()->event_serialization, static_cast<uint8_t>(SerializationType::JSON));
  EXPECT_STREQ(found_pv->get()->pv_protocol.c_str(), "pva");
  EXPECT_STREQ(found_pv->get()->pv_destination.c_str(), "dest");
  EXPECT_EQ(found_pv->get()->requested_instance, 2);

  EXPECT_NO_THROW(toShared(storage->getPVRepository())->remove("channel::a", "dest"););
  found_pv = toShared(storage->getPVRepository())->getPVMonitor("channel::a", "dest");
  EXPECT_EQ(found_pv->get()->requested_instance, 1);

  EXPECT_NO_THROW(toShared(storage->getPVRepository())->remove("channel::a", "dest"););
  EXPECT_EQ(toShared(storage->getPVRepository())->isPresent("channel::a", "dest"), false);
}

TEST(DataStorage, MultiThreading) {
  const int                    test_element = 10;
  std::shared_ptr<DataStorage> storage;
  std::vector<std::thread>     thread_vec;
  std::latch                   test_done(test_element);
  EXPECT_NO_THROW(storage = std::make_shared<DataStorage>(fs::path(fs::current_path()) / "test.sqlite"););
  EXPECT_NO_THROW(toShared(storage->getPVRepository())->removeAll(););
  SCOPED_TRACE("Start thread");
  for (int idx = 0; idx < test_element; idx++) {
    thread_vec.push_back(std::move(std::thread([t_id = idx, &t_storage = storage, &t_latch = test_done] {
      std::string pv_name = "channel::" + std::to_string(t_id);
      EXPECT_NO_THROW(toShared(t_storage->getPVRepository())->insert({.pv_name = pv_name, .pv_protocol = "pv", .pv_destination = "dest"}););
      auto found_pv = toShared(t_storage->getPVRepository())->getPVMonitor(pv_name, "dest");

      EXPECT_EQ(found_pv.has_value(), true);
      EXPECT_STREQ(found_pv->get()->pv_name.c_str(), pv_name.c_str());
      EXPECT_STREQ(found_pv->get()->pv_protocol.c_str(), "pv");
      EXPECT_STREQ(found_pv->get()->pv_destination.c_str(), "dest");
      t_latch.count_down();
    })));
  }
  SCOPED_TRACE("Wait thread");
  test_done.wait();
  std::for_each(thread_vec.begin(), thread_vec.end(), [](auto& t) { t.join(); });
  thread_vec.clear();
}

TEST(DataStorage, ChannelProcessingHandler) {
  const int                    test_element = 10;
  std::shared_ptr<DataStorage> storage;
  EXPECT_NO_THROW(storage = std::make_shared<DataStorage>(fs::path(fs::current_path()) / "test.sqlite"););
  EXPECT_NO_THROW(toShared(storage->getPVRepository())->removeAll(););
  std::vector<uint32_t> all_index;
  for (int idx = 0; idx < 100; idx++) {
    all_index.push_back(idx);
    EXPECT_NO_THROW(
        toShared(storage->getPVRepository())->insert({.pv_name = "channel", .pv_protocol = "pv", .pv_destination = "dest_" + std::to_string(idx)}););
  }

  // check distinct, only one need to be found
  PVMonitorDistinctResultType distinct_result;
  EXPECT_NO_THROW(distinct_result = toShared(storage->getPVRepository())->getDistinctByNameProtocol());
  EXPECT_EQ(distinct_result.size(), 1);

  // check processing
  EXPECT_NO_THROW(toShared(storage->getPVRepository())
                      ->processAllPVMonitor(
                          std::get<0>(distinct_result[0]), std::get<1>(distinct_result[0]), [&all_index = all_index](uint32_t index, auto& pv_description) {
                            ASSERT_NE(std::find(std::begin(all_index), std::end(all_index), index), std::end(all_index));
                            EXPECT_STREQ(pv_description.pv_destination.c_str(), ("dest_" + std::to_string(index)).c_str());
                          }););
}
