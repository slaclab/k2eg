#include <gtest/gtest.h>
#include <k2eg/k2eg.h>
#include <k2eg/common/ProgramOptions.h>

#include <filesystem>
#include <iostream>
#include <fstream>
#include <string>

using namespace k2eg;
namespace fs = std::filesystem;

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

    setenv(std::string("EPICS_k2eg_").append(CMD_INPUT_TOPIC).c_str(), "cmd_topic_in", 1);
    setenv(std::string("EPICS_k2eg_").append(PUB_SERVER_ADDRESS).c_str(), "kafka:9092", 1);
    setenv(std::string("EPICS_k2eg_").append(SUB_SERVER_ADDRESS).c_str(), "kafka:9092", 1);
    setenv(std::string("EPICS_k2eg_").append(STORAGE_PATH).c_str(), storage_db_file.c_str(), 1);

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
    EXPECT_NE(full_str.find("Shoutdown compelted"), std::string::npos);
}
#endif //__linux__