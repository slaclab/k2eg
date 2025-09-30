#include <k2eg/common/utility.h>
#include <k2eg/service/data/DataStorage.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
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
                                  .event_serialization = static_cast<uint8_t>(SerializationType::JSON),
                                //   .channel_protocol = "pva",
                                  .channel_destination = "dest"}););
    auto found_channel = toShared(storage->getChannelRepository())
                             ->getChannelMonitor({.pv_name = "channel::a",
                                                  .channel_destination = "dest"});

    EXPECT_EQ(found_channel.has_value(), true);
    EXPECT_STREQ(found_channel->get()->pv_name.c_str(), "channel::a");
    EXPECT_EQ(found_channel->get()->event_serialization, static_cast<uint8_t>(SerializationType::JSON));
    // EXPECT_STREQ(found_channel->get()->channel_protocol.c_str(), "pva");
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
                                            //   .channel_protocol = "pv",
                                              .channel_destination = "dest"}););
                auto found_channel =
                    toShared(t_storage->getChannelRepository())
                        ->getChannelMonitor({.pv_name = pv_name,
                                            //  .channel_protocol = "pv",
                                             .channel_destination = "dest"});

                EXPECT_EQ(found_channel.has_value(), true);
                EXPECT_STREQ(found_channel->get()->pv_name.c_str(),
                             pv_name.c_str());
                // EXPECT_STREQ(found_channel->get()->channel_protocol.c_str(), "pv");
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
                        //   .channel_protocol = "pv",
                          .channel_destination = "dest_" + std::to_string(idx)}););
    }

    //check distinct, only one need to be found
    ChannelMonitorDistinctResultType distinct_result;
    EXPECT_NO_THROW(distinct_result = toShared(storage->getChannelRepository())->getDistinctByNameProtocol());
    EXPECT_EQ(distinct_result.size(), 1);

    // check processing
    EXPECT_NO_THROW(toShared(storage->getChannelRepository())->processAllChannelMonitor(
        std::get<0>(distinct_result[0]),
        [&all_index = all_index](uint32_t index, auto &channel_description) {
            ASSERT_NE(std::find(std::begin(all_index), std::end(all_index), index), std::end(all_index));
            EXPECT_STREQ(channel_description.channel_destination.c_str(), ("dest_" + std::to_string(index)).c_str());
        }
    ););
}

TEST(DataStorage, ChannelProcessingHandlerLimitedNumberOfItem) {
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
                        //   .channel_protocol = "pv",
                          .channel_destination = "dest_" + std::to_string(idx)}););
    }

    //check distinct, only one need to be found
    ChannelMonitorDistinctResultType distinct_result;
    EXPECT_NO_THROW(distinct_result = toShared(storage->getChannelRepository())->getDistinctByNameProtocol());
    EXPECT_EQ(distinct_result.size(), 1);

    // check processing first 10 element
    std::set<uint32_t> procesed_element;
    EXPECT_NO_THROW(toShared(storage->getChannelRepository())->processUnprocessedChannelMonitor(
        std::get<0>(distinct_result[0]),
        10,
        [&procesed_element](uint32_t index, auto &channel_description) {
           
            ASSERT_TRUE(index<10);
            ASSERT_TRUE(procesed_element.insert(channel_description.id).second);
        }
    ););
    // check processing next 10 element
    EXPECT_NO_THROW(toShared(storage->getChannelRepository())->processUnprocessedChannelMonitor(
        std::get<0>(distinct_result[0]),
        10,
        [&procesed_element](uint32_t index, auto &channel_description) {
            ASSERT_TRUE(index<10);
            ASSERT_TRUE(procesed_element.insert(channel_description.id).second);
        }
    ););

    // reset the processing state and restart from the beginning
    EXPECT_NO_THROW(toShared(storage->getChannelRepository())->resetProcessStateChannel(std::get<0>(distinct_result[0])));
    // in this case all the insert should gone wrong
        EXPECT_NO_THROW(toShared(storage->getChannelRepository())->processUnprocessedChannelMonitor(
        std::get<0>(distinct_result[0]),
        10,
        [&procesed_element](uint32_t index, auto &channel_description) {
           
            ASSERT_TRUE(index<10);
            ASSERT_FALSE(procesed_element.insert(channel_description.id).second);
        }
    ););
    // check processing next 10 element
    EXPECT_NO_THROW(toShared(storage->getChannelRepository())->processUnprocessedChannelMonitor(
        std::get<0>(distinct_result[0]),
        10,
        [&procesed_element](uint32_t index, auto &channel_description) {
            ASSERT_TRUE(index<10);
            ASSERT_FALSE(procesed_element.insert(channel_description.id).second);
        }
    ););

}

#include <ios>
#include <iostream>
#include <fstream>
void process_mem_usage(double& vm_usage, double& resident_set)
{
   using std::ios_base;
   using std::ifstream;
   using std::string;

   vm_usage     = 0.0;
   resident_set = 0.0;

   // 'file' stat seems to give the most reliable results
   //
   ifstream stat_stream("/proc/self/stat",ios_base::in);

   // dummy vars for leading entries in stat that we don't care about
   //
   string pid, comm, state, ppid, pgrp, session, tty_nr;
   string tpgid, flags, minflt, cminflt, majflt, cmajflt;
   string utime, stime, cutime, cstime, priority, nice;
   string O, itrealvalue, starttime;

   // the two fields we want
   //
   unsigned long vsize;
   long rss;

   stat_stream >> pid >> comm >> state >> ppid >> pgrp >> session >> tty_nr
               >> tpgid >> flags >> minflt >> cminflt >> majflt >> cmajflt
               >> utime >> stime >> cutime >> cstime >> priority >> nice
               >> O >> itrealvalue >> starttime >> vsize >> rss; // don't care about the rest

   stat_stream.close();

   long page_size_kb = sysconf(_SC_PAGE_SIZE) / 1024; // in case x86-64 is configured to use 2MB pages
   vm_usage     = vsize / 1024.0;
   resident_set = rss * page_size_kb;
}


// TEST(DataStorage, TestMemoryLeaks) {
//     std::shared_ptr<DataStorage> storage;
//     EXPECT_NO_THROW(storage = std::make_shared<DataStorage>(fs::path(fs::current_path())
//                                                             / "test.sqlite"););
//     double vm, rss;
//     double vm_end, rss_end;

//     process_mem_usage(vm, rss);
//     auto channel_repository = toShared(storage->getChannelRepository());
//     for(int idx = 0; idx < 10000; idx++) {
//         auto distinct_name_prot = channel_repository->getDistinctByNameProtocol();
//     }
//     storage->freeMemory();
//     process_mem_usage(vm_end, rss_end);
//     int vm_diff = vm_end - vm;
//     int rss_diff = rss_end - rss;
// }
