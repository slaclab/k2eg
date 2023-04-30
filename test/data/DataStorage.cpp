#include <k2eg/common/utility.h>
#include <k2eg/service/data/DataStorage.h>
#include <gtest/gtest.h>

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
    EXPECT_NO_THROW(storage = std::make_unique<DataStorage>(fs::path(fs::current_path())
                                                            / "test.sqlite"););
    EXPECT_NO_THROW(toShared(storage->getChannelRepository())->removeAll(););
    EXPECT_NO_THROW(toShared(storage->getChannelRepository())
                        ->insert({.pv_name = "channel::a",
                                  .event_serialization = static_cast<uint8_t>(MessageSerType::json),
                                  .channel_protocol = "pva",
                                  .channel_destination = "dest"}););
    auto found_channel = toShared(storage->getChannelRepository())
                             ->getChannelMonitor({.pv_name = "channel::a",
                                                  .channel_destination = "dest"});

    EXPECT_EQ(found_channel.has_value(), true);
    EXPECT_STREQ(found_channel->get()->pv_name.c_str(), "channel::a");
    EXPECT_EQ(found_channel->get()->event_serialization, static_cast<uint8_t>(MessageSerType::json));
    EXPECT_STREQ(found_channel->get()->channel_protocol.c_str(), "pva");
    EXPECT_STREQ(found_channel->get()->channel_destination.c_str(), "dest");
}

TEST(DataStorage, MultiThreading) {
    const int test_element = 10;
    std::shared_ptr<DataStorage> storage;
    std::vector<std::thread> thread_vec;
    std::latch test_done(test_element);
    EXPECT_NO_THROW(storage = std::make_shared<DataStorage>(fs::path(fs::current_path())
                                                            / "test.sqlite"););
    EXPECT_NO_THROW(toShared(storage->getChannelRepository())->removeAll(););
    SCOPED_TRACE("Start thread");
    for (int idx = 0; idx < test_element; idx++) {
        thread_vec.push_back(std::move(
            std::thread([t_id = idx, &t_storage = storage, &t_latch = test_done] {
                std::string pv_name = "channel::" + std::to_string(t_id);
                EXPECT_NO_THROW(toShared(t_storage->getChannelRepository())
                                    ->insert({.pv_name = pv_name,
                                              .channel_protocol = "pv",
                                              .channel_destination = "dest"}););
                auto found_channel =
                    toShared(t_storage->getChannelRepository())
                        ->getChannelMonitor({.pv_name = pv_name,
                                             .channel_protocol = "pv",
                                             .channel_destination = "dest"});

                EXPECT_EQ(found_channel.has_value(), true);
                EXPECT_STREQ(found_channel->get()->pv_name.c_str(),
                             pv_name.c_str());
                EXPECT_STREQ(found_channel->get()->channel_protocol.c_str(), "pv");
                EXPECT_STREQ(found_channel->get()->channel_destination.c_str(), "dest");
                t_latch.count_down();
            })));
    }
    SCOPED_TRACE("Wait thread");
    test_done.wait();
    std::for_each(thread_vec.begin(), thread_vec.end(), [](auto& t) { t.join(); });
    thread_vec.clear();
}

TEST(DataStorage, ChannelProcessingHandler) {
    const int test_element = 10;
    std::shared_ptr<DataStorage> storage;
    EXPECT_NO_THROW(storage = std::make_shared<DataStorage>(fs::path(fs::current_path())
                                                            / "test.sqlite"););
    EXPECT_NO_THROW(toShared(storage->getChannelRepository())->removeAll(););
    std::vector<uint32_t> all_index;
    for (int idx = 0; idx < 100; idx++) {
        all_index.push_back(idx);
        EXPECT_NO_THROW(
            toShared(storage->getChannelRepository())
                ->insert({.pv_name = "channel",
                          .channel_protocol = "pv",
                          .channel_destination = "dest_" + std::to_string(idx)}););
    }

    //check distinct, only one need to be found
    ChannelMonitorDistinctResultType distinct_result;
    EXPECT_NO_THROW(distinct_result = toShared(storage->getChannelRepository())->getDistinctByNameProtocol());
    EXPECT_EQ(distinct_result.size(), 1);

    // check processing
    EXPECT_NO_THROW(toShared(storage->getChannelRepository())->processAllChannelMonitor(
        std::get<0>(distinct_result[0]),
        std::get<1>(distinct_result[0]),
        [&all_index = all_index](uint32_t index, auto &channel_description) {
            ASSERT_NE(std::find(std::begin(all_index), std::end(all_index), index), std::end(all_index));
            EXPECT_STREQ(channel_description.channel_destination.c_str(), ("dest_" + std::to_string(index)).c_str());
        }
    ););
}
