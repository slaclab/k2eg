
#include <gtest/gtest.h>

#include <ctime>
#include <k2eg/common/ProgramOptions.h>
#include <k2eg/common/utility.h>
#include <k2eg/controller/node/NodeController.h>
#include <k2eg/service/ServiceResolver.h>
#include <k2eg/service/data/DataStorage.h>
#include <k2eg/service/epics/EpicsServiceManager.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/log/impl/BoostLogger.h>
#include <k2eg/service/pubsub/pubsub.h>
#include <filesystem>
#include <latch>
#include <random>

namespace fs = std::filesystem;

using namespace k2eg::common;

using namespace k2eg::controller::command;
using namespace k2eg::controller::node;

using namespace k2eg::service;
using namespace k2eg::service::log;
using namespace k2eg::service::log::impl;
using namespace k2eg::service::pubsub;
using namespace k2eg::service::data;
using namespace k2eg::service::epics_impl;

using namespace k2eg::service::pubsub;
using namespace k2eg::service::pubsub::impl::kafka;

#define KAFKA_HOSTNAME "kafka:9092"
#define KAFKA_TOPIC_ACQUIRE_IN "acquire_commad_in"

class DummyPublisher : public IPublisher {
    std::latch& lref;
    PublisherMessageVector sent_messages;

public:
    DummyPublisher(std::latch& lref):IPublisher(std::make_unique<const PublisherConfiguration>(PublisherConfiguration{.server_address="fake_address"})), lref(lref){};
    ~DummyPublisher() = default;
    void setAutoPoll(bool autopoll) {}
    int setCallBackForReqType(const std::string req_type, EventCallback eventCallback) { return 0; }
    int createQueue(const std::string& queue) { return 0; }
    int flush(const int timeo) { return 0; }
    int pushMessage(PublishMessageUniquePtr message) {
        sent_messages.push_back(std::move(message));
        lref.count_down();
        return 0;
    }
    int pushMessages(PublisherMessageVector& messages) {
        for (auto& uptr: messages) {
            sent_messages.push_back(std::move(uptr));
            lref.count_down();
        }
        return 0;
    }
    size_t getQueueMessageSize() { return sent_messages.size(); }
};

int random_num(int min, int max) {
    std::minstd_rand generator(std::time(0));
    std::uniform_int_distribution<> dist(min, max);
    return dist(generator);
}
#ifdef __linux__
TEST(NodeController, AcquireCommand) {
    int argc = 1;
    const char* argv[1] = {"epics-k2eg-test"};
    DataStorageUPtr storage;
    std::unique_ptr<NodeController> cmd_controller;
    std::latch work_done{1};
    // set environment variable for test
    clearenv();
    setenv("EPICS_k2eg_log-on-console", "false", 1);
    std::unique_ptr<ProgramOptions> opt = std::make_unique<ProgramOptions>();
    ASSERT_NO_THROW(opt->parse(argc, argv));
    // configure the services
    ASSERT_NO_THROW(
        ServiceResolver<ILogger>::registerService(std::make_shared<BoostLogger>(opt->getloggerConfiguration())););
    ASSERT_NO_THROW(ServiceResolver<EpicsServiceManager>::registerService(std::make_shared<EpicsServiceManager>()););
    ASSERT_NO_THROW(ServiceResolver<IPublisher>::registerService(std::make_shared<DummyPublisher>(work_done)););

    EXPECT_NO_THROW(storage = std::make_unique<DataStorage>(fs::path(fs::current_path()) / "test.sqlite"););
    toShared(storage->getChannelRepository())->removeAll();
    EXPECT_NO_THROW(cmd_controller = std::make_unique<NodeController>(std::move(storage)););

    EXPECT_NO_THROW(cmd_controller->submitCommand({std::make_shared<const AquireCommand>(
        AquireCommand{CommandType::monitor, "pva", "channel:ramp:ramp", true, KAFKA_TOPIC_ACQUIRE_IN})}););

    work_done.wait();
    // we need to have publish some message
    size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_NE(published, 0);

    // stop acquire
    EXPECT_NO_THROW(cmd_controller->submitCommand({std::make_shared<const AquireCommand>(
        AquireCommand{CommandType::monitor, "", "channel:ramp:ramp", false, KAFKA_TOPIC_ACQUIRE_IN})}););

    sleep(1);
    EXPECT_NO_THROW(published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize(););
    sleep(2);
    EXPECT_EQ(ServiceResolver<IPublisher>::resolve()->getQueueMessageSize(), published);
    // dispose all
    cmd_controller.reset();
    EXPECT_NO_THROW(ServiceResolver<IPublisher>::resolve().reset(););
    EXPECT_NO_THROW(ServiceResolver<EpicsServiceManager>::resolve().reset(););
    EXPECT_NO_THROW(ServiceResolver<ILogger>::resolve().reset(););
}

TEST(NodeController, AcquireCommandAfterReboot) {
    int argc = 1;
    const char* argv[1] = {"epics-k2eg-test"};
    DataStorageUPtr storage;
    std::unique_ptr<NodeController> node_controller;
    std::latch work_done{1};
    std::latch work_done_2{1};
    // set environment variable for test
    clearenv();
    setenv("EPICS_k2eg_log-on-console", "false", 1);
    std::unique_ptr<ProgramOptions> opt = std::make_unique<ProgramOptions>();
    ASSERT_NO_THROW(opt->parse(argc, argv));
    // configure the services
    ASSERT_NO_THROW(
        ServiceResolver<ILogger>::registerService(std::make_shared<BoostLogger>(opt->getloggerConfiguration())););
    ASSERT_NO_THROW(ServiceResolver<EpicsServiceManager>::registerService(std::make_shared<EpicsServiceManager>()););
    ASSERT_NO_THROW(ServiceResolver<IPublisher>::registerService(std::make_shared<DummyPublisher>(work_done)););

    EXPECT_NO_THROW(storage = std::make_unique<DataStorage>(fs::path(fs::current_path()) / "test.sqlite"););
    toShared(storage->getChannelRepository())->removeAll();
    EXPECT_NO_THROW(node_controller = std::make_unique<NodeController>(std::move(storage)););

    EXPECT_NO_THROW(node_controller->submitCommand({std::make_shared<const AquireCommand>(
        AquireCommand{CommandType::monitor, "pva", "channel:ramp:ramp", true, KAFKA_TOPIC_ACQUIRE_IN})}););

    work_done.wait();
    // we need to have publish some message
    size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_NE(published, 0);

    // stop the node controller
    node_controller.reset();
    EXPECT_NO_THROW(ServiceResolver<IPublisher>::resolve().reset(););
    EXPECT_NO_THROW(ServiceResolver<EpicsServiceManager>::resolve().reset(););
    EXPECT_NO_THROW(ServiceResolver<ILogger>::resolve().reset(););

    // reboot
    ASSERT_NO_THROW(
        ServiceResolver<ILogger>::registerService(std::make_shared<BoostLogger>(opt->getloggerConfiguration())););
    ASSERT_NO_THROW(ServiceResolver<EpicsServiceManager>::registerService(std::make_shared<EpicsServiceManager>()););
    ASSERT_NO_THROW(ServiceResolver<IPublisher>::registerService(std::make_shared<DummyPublisher>(work_done_2)););
    EXPECT_NO_THROW(storage = std::make_unique<DataStorage>(fs::path(fs::current_path()) / "test.sqlite"););
    EXPECT_NO_THROW(node_controller = std::make_unique<NodeController>(std::move(storage)););
    EXPECT_NO_THROW(node_controller->reloadPersistentCommand(););

    work_done_2.wait();
    // we need to have publish some message
    published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_NE(published, 0);

    // dispose all
    node_controller.reset();
    EXPECT_NO_THROW(ServiceResolver<IPublisher>::resolve().reset(););
    EXPECT_NO_THROW(ServiceResolver<EpicsServiceManager>::resolve().reset(););
    EXPECT_NO_THROW(ServiceResolver<ILogger>::resolve().reset(););
}

TEST(NodeController, GetCommand) {
    int argc = 1;
    const char* argv[1] = {"epics-k2eg-test"};
    DataStorageUPtr storage;
    std::unique_ptr<NodeController> cmd_controller;
    std::latch work_done{1};
    // set environment variable for test
    clearenv();
    setenv("EPICS_k2eg_log-on-console", "false", 1);
    std::unique_ptr<ProgramOptions> opt = std::make_unique<ProgramOptions>();
    ASSERT_NO_THROW(opt->parse(argc, argv));
    // configure the services
    ASSERT_NO_THROW(
        ServiceResolver<ILogger>::registerService(std::make_shared<BoostLogger>(opt->getloggerConfiguration())););
    ASSERT_NO_THROW(ServiceResolver<EpicsServiceManager>::registerService(std::make_shared<EpicsServiceManager>()););
    ASSERT_NO_THROW(ServiceResolver<IPublisher>::registerService(std::make_shared<DummyPublisher>(work_done)););

    EXPECT_NO_THROW(storage = std::make_unique<DataStorage>(fs::path(fs::current_path()) / "test.sqlite"););
    toShared(storage->getChannelRepository())->removeAll();
    EXPECT_NO_THROW(cmd_controller = std::make_unique<NodeController>(std::move(storage)););

    EXPECT_NO_THROW(cmd_controller->submitCommand({std::make_shared<const GetCommand>(
        GetCommand{CommandType::get, "pva", "channel:ramp:ramp", KAFKA_TOPIC_ACQUIRE_IN})}););

    work_done.wait();
    // we need to have publish some message
    size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_NE(published, 0);

    // dispose all
    cmd_controller.reset();
    EXPECT_NO_THROW(ServiceResolver<IPublisher>::resolve().reset(););
    EXPECT_NO_THROW(ServiceResolver<EpicsServiceManager>::resolve().reset(););
    EXPECT_NO_THROW(ServiceResolver<ILogger>::resolve().reset(););
}

TEST(NodeController, GetCommandBadChannel) {
    int argc = 1;
    const char* argv[1] = {"epics-k2eg-test"};
    DataStorageUPtr storage;
    std::unique_ptr<NodeController> cmd_controller;
    std::latch work_done{1};
    // set environment variable for test
    clearenv();
    setenv("EPICS_k2eg_log-on-console", "false", 1);
    std::unique_ptr<ProgramOptions> opt = std::make_unique<ProgramOptions>();
    ASSERT_NO_THROW(opt->parse(argc, argv));
    // configure the services
    ASSERT_NO_THROW(
        ServiceResolver<ILogger>::registerService(std::make_shared<BoostLogger>(opt->getloggerConfiguration())););
    ASSERT_NO_THROW(ServiceResolver<EpicsServiceManager>::registerService(std::make_shared<EpicsServiceManager>()););
    ASSERT_NO_THROW(ServiceResolver<IPublisher>::registerService(std::make_shared<DummyPublisher>(work_done)););

    EXPECT_NO_THROW(storage = std::make_unique<DataStorage>(fs::path(fs::current_path()) / "test.sqlite"););
    toShared(storage->getChannelRepository())->removeAll();
    EXPECT_NO_THROW(cmd_controller = std::make_unique<NodeController>(std::move(storage)););

    EXPECT_NO_THROW(cmd_controller->submitCommand({std::make_shared<const GetCommand>(
        GetCommand{CommandType::get, "pva", "bad:channel:name", KAFKA_TOPIC_ACQUIRE_IN})}););
    //give some time for the timeout
    sleep(5);
    // we need to have publish some message
    size_t published = ServiceResolver<IPublisher>::resolve()->getQueueMessageSize();
    EXPECT_EQ(published, 0);

    // dispose all
    cmd_controller.reset();
    EXPECT_NO_THROW(ServiceResolver<IPublisher>::resolve().reset(););
    EXPECT_NO_THROW(ServiceResolver<EpicsServiceManager>::resolve().reset(););
    EXPECT_NO_THROW(ServiceResolver<ILogger>::resolve().reset(););
}
#endif // __linux__
TEST(NodeController, RandomCommand) {
    int argc = 1;
    const char* argv[1] = {"epics-k2eg-test"};
    DataStorageUPtr storage;
    std::unique_ptr<NodeController> cmd_controller;
    std::latch work_done{1};
    // set environment variable for test
    setenv("EPICS_k2eg_log-on-console", "false", 1);
    std::unique_ptr<ProgramOptions> opt = std::make_unique<ProgramOptions>();
    ASSERT_NO_THROW(opt->parse(argc, argv));
    // configure the services
    ASSERT_NO_THROW(
        ServiceResolver<ILogger>::registerService(std::make_shared<BoostLogger>(opt->getloggerConfiguration())););
    ASSERT_NO_THROW(ServiceResolver<EpicsServiceManager>::registerService(std::make_shared<EpicsServiceManager>()););
    ASSERT_NO_THROW(ServiceResolver<IPublisher>::registerService(std::make_shared<DummyPublisher>(work_done)););

    EXPECT_NO_THROW(storage = std::make_unique<DataStorage>(fs::path(fs::current_path()) / "test.sqlite"););
    toShared(storage->getChannelRepository())->removeAll();
    EXPECT_NO_THROW(cmd_controller = std::make_unique<NodeController>(std::move(storage)););

    // send 100 random commands equence iteration
    for (int idx = 0; idx < 100; idx++) {
        switch (random_num(0, 2)) {
        case 0: {
            EXPECT_NO_THROW(cmd_controller->submitCommand({std::make_shared<const AquireCommand>(
                AquireCommand{CommandType::monitor, "pva", "channel:ramp:ramp", true, KAFKA_TOPIC_ACQUIRE_IN})}););
            break;
        }
        case 1: {
            EXPECT_NO_THROW(cmd_controller->submitCommand({std::make_shared<const AquireCommand>(
                AquireCommand{CommandType::monitor, "pva", "channel:ramp:ramp", false, KAFKA_TOPIC_ACQUIRE_IN})}););
            break;
        }
        case 2: {
            EXPECT_NO_THROW(cmd_controller->submitCommand({std::make_shared<const GetCommand>(
                GetCommand{CommandType::get, "pva", "channel:ramp:ramp", KAFKA_TOPIC_ACQUIRE_IN})}););
            break;
        }
        }
    }
    // dispose all
    cmd_controller.reset();
    EXPECT_NO_THROW(ServiceResolver<IPublisher>::resolve().reset(););
    EXPECT_NO_THROW(ServiceResolver<EpicsServiceManager>::resolve().reset(););
    EXPECT_NO_THROW(ServiceResolver<ILogger>::resolve().reset(););
}