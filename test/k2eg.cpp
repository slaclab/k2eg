#include <gtest/gtest.h>
#include <k2eg/k2eg.h>
#include <k2eg/common/ProgramOptions.h>
#include <k2eg/common/uuid.h>
#include <k2eg/service/pubsub/pubsub.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>

#include "pubsub/common.h"

using namespace k2eg;
namespace fs = std::filesystem;
using namespace k2eg::service::pubsub;
using namespace k2eg::service::pubsub::impl::kafka;

#define TEST_TOPIC_IN "cmd_topic_in"
#define TEST_TOPIC_OUT "cmd_topic_out"
TEST(k2egateway, NoParam)
{
    int argc = 1;
    const char *argv[1] = {"epics-k2eg-test"};
    setenv(std::string("EPICS_k2eg_").append(LOG_ON_CONSOLE).c_str(), "false", 1);
    auto k2eg = std::make_unique<K2EGateway>();
     std::thread t([&k2eg = k2eg, &argc = argc, &argv = argv](){
         int exit_code = k2eg->run(argc, argv);
         EXPECT_EQ(exit_code, 1);
    });
    sleep(2);
    k2eg->stop();
    t.join();
}

#ifdef __linux__
TEST(k2egateway, Default)
{
    int argc = 1;
    const char *argv[1] = {"epics-k2eg-test"};
    const std::string log_file_path = fs::path(fs::current_path()) / "log-file-test.log";
    const std::string storage_db_file = fs::path(fs::current_path()) / "k2eg.sqlite";

    // set environment variable for test
    clearenv();
    setenv(std::string("EPICS_k2eg_").append(LOG_ON_CONSOLE).c_str(), "false", 1);
    setenv(std::string("EPICS_k2eg_").append(LOG_LEVEL).c_str(), "info", 1);
    setenv(std::string("EPICS_k2eg_").append(LOG_ON_FILE).c_str(), "true", 1);
    setenv(std::string("EPICS_k2eg_").append(LOG_FILE_NAME).c_str(), log_file_path.c_str(), 1);

    setenv(std::string("EPICS_k2eg_").append(CMD_INPUT_TOPIC).c_str(), TEST_TOPIC_IN, 1);
    setenv(std::string("EPICS_k2eg_").append(PUB_SERVER_ADDRESS).c_str(), "kafka:9092", 1);
    setenv(std::string("EPICS_k2eg_").append(SUB_SERVER_ADDRESS).c_str(), "kafka:9092", 1);
    setenv(std::string("EPICS_k2eg_").append(STORAGE_PATH).c_str(), storage_db_file.c_str(), 1);
    setenv(std::string("EPICS_k2eg_").append(METRIC_HTTP_PORT).c_str(), "8081", 1);
    setenv(std::string("EPICS_k2eg_").append(CONFIGURATION_SERVICE_HOST).c_str(), "consul", 1);
    // remove possible old file
    std::filesystem::remove(log_file_path);
    auto k2eg = std::make_unique<K2EGateway>();
    std::thread t([&k2eg = k2eg, &argc = argc, &argv = argv](){
         int exit_code = k2eg->run(argc, argv);
         EXPECT_EQ(k2eg->isStopRequested(), true);
         EXPECT_EQ(k2eg->isTerminated(), true);
         EXPECT_EQ(exit_code, EXIT_SUCCESS);
    });
    sleep(2);
    k2eg->stop();
    t.join();

    // read file for test
    std::ifstream ifs; // input file stream
    std::string str;
    std::string full_str;
    ifs.open(log_file_path, std::ios::in);
    if (ifs)
    {
        while (!ifs.eof())
        {
            std::getline(ifs, str);
            full_str.append(str);
        }
        ifs.close();
    }
    EXPECT_NE(full_str.find("Shutdown completed"), std::string::npos);
    if(full_str.find("Shutdown completed") == std::string::npos) {
        std::cerr << "Log file content: " << full_str << std::endl;
    }
}

TEST(k2egateway, CommandSubmission)
{
    int argc = 1;
    const char *argv[1] = {"epics-k2eg-test"};
    const std::string log_file_path = fs::path(fs::current_path()) / "log-file-test.log";
    const std::string storage_db_file = fs::path(fs::current_path()) / "k2eg.sqlite";
    SubscriberInterfaceElementVector  messages;
    // set environment variable for test
    clearenv();
    setenv(std::string("EPICS_k2eg_").append(LOG_ON_CONSOLE).c_str(), "false", 1);
    setenv(std::string("EPICS_k2eg_").append(LOG_LEVEL).c_str(), "info", 1);
    setenv(std::string("EPICS_k2eg_").append(LOG_ON_FILE).c_str(), "true", 1);
    setenv(std::string("EPICS_k2eg_").append(LOG_FILE_NAME).c_str(), log_file_path.c_str(), 1);

    setenv(std::string("EPICS_k2eg_").append(CMD_INPUT_TOPIC).c_str(), TEST_TOPIC_IN, 1);
    setenv(std::string("EPICS_k2eg_").append(PUB_SERVER_ADDRESS).c_str(), "kafka:9092", 1);
    setenv(std::string("EPICS_k2eg_").append(SUB_SERVER_ADDRESS).c_str(), "kafka:9092", 1);
    // set random group
    setenv(std::string("EPICS_k2eg_").append(SUB_GROUP_ID).c_str(), common::UUID::generateUUIDLite().c_str(), 1);
    setenv(std::string("EPICS_k2eg_").append(STORAGE_PATH).c_str(), storage_db_file.c_str(), 1);
    setenv(std::string("EPICS_k2eg_").append(METRIC_HTTP_PORT).c_str(), "8081", 1);
    setenv(std::string("EPICS_k2eg_").append(CONFIGURATION_SERVICE_HOST).c_str(), "consul", 1);
    // remove possible old file
    std::filesystem::remove(log_file_path);
    auto k2eg = std::make_unique<K2EGateway>();
    std::thread t([&k2eg = k2eg, &argc = argc, &argv = argv](){
         int exit_code = k2eg->run(argc, argv);
         EXPECT_EQ(k2eg->isStopRequested(), true);
         EXPECT_EQ(k2eg->isTerminated(), true);
         EXPECT_EQ(exit_code, EXIT_SUCCESS);
    });
    
    // create consumer and producer to send comamnd adn wait for reply to/from kafka
    std::unique_ptr<RDKafkaPublisher> producer = std::make_unique<RDKafkaPublisher>(std::make_unique<const PublisherConfiguration>(PublisherConfiguration{.server_address = "kafka:9092"}));
    std::unique_ptr<RDKafkaSubscriber> consumer = std::make_unique<RDKafkaSubscriber>(std::make_unique<const SubscriberConfiguration>(SubscriberConfiguration{.server_address = "kafka:9092"}));
    ASSERT_NO_THROW(consumer->setQueue({TEST_TOPIC_OUT}));
    // drain old messages
    ASSERT_EQ(consumer->getMsg(messages, 1000, 1000), 0);
    messages.clear();
    // generate json for get command using boost json
    boost::json::object get_json_obj;
    get_json_obj["command"] = "get";
    get_json_obj["reply_id"] = "REPLY_ID_JSON";
    get_json_obj["reply_topic"] = TEST_TOPIC_OUT;
    get_json_obj["pv_name"] = "pva://variable:a";
    get_json_obj["serialization"] = "json";

    ASSERT_EQ(producer->createQueue(
        QueueDescription{
          .name = TEST_TOPIC_IN,
          .paritions = 1,
          .retention_time = 1000*60*60,
          .retention_size = 1024*2
        }
      ), 0);
      ASSERT_EQ(producer->createQueue(
        QueueDescription{
          .name = TEST_TOPIC_OUT,
          .paritions = 1,
          .retention_time = 1000*60*60,
          .retention_size = 1024*2
        }
      ), 0);
      sleep(5);
      auto iotaFuture = std::async(
          std::launch::async,
          [&get_json_obj](std::unique_ptr<IPublisher> producer) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            ASSERT_EQ(producer->pushMessage(std::move(PublishMessageUniquePtr(new Message(TEST_TOPIC_IN, boost::json::serialize(get_json_obj))))), 0);
            ASSERT_EQ(producer->flush(1000), 0);
          },
          std::move(producer));
    // wait untli
    iotaFuture.wait();
    // now wait for execution of the command
    int retry = 0;
    while (messages.size() == 0 && retry < 10) {
        ASSERT_EQ(consumer->getMsg(messages, 1, 1000), 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        retry++;
    }
    ASSERT_NE(messages.size(), 0);
    // decode get json message
    boost::json::object get_json_reply_obj;
    boost::json::string_view value_str = boost::json::string_view(messages[0]->data.get(), messages[0]->data_len);
    
    ASSERT_NO_THROW(get_json_reply_obj = boost::json::parse(value_str).as_object(););
    // need to contains "error" field
    ASSERT_TRUE(get_json_reply_obj.contains("error"));
    // need to contains "reply_id" field
    ASSERT_TRUE(get_json_reply_obj.contains("reply_id"));
    // neeed to contains the pv name
    ASSERT_TRUE(get_json_reply_obj.contains("variable:a"));
    producer.reset();
    consumer.reset();
    k2eg->stop();
    t.join();
}

#endif //__linux__