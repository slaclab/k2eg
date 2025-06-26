
#include <gtest/gtest.h>

#include <filesystem>
#include <stdlib.h>
#include <string>

#include <k2eg/common/ProgramOptions.h>

using namespace k2eg::common;
using namespace k2eg::controller::node;
using namespace k2eg::service::pubsub;
namespace fs = std::filesystem;
#ifdef __linux__
TEST(ProgramOptions, CPPStandardOptions) {
    int argc = 1;
    const char* argv[1] = {"epics-k2eg-test"};
    // set environment variable for test
    clearenv();
    setenv("EPICS_k2eg_log-level", "debug", 1);
    std::unique_ptr<ProgramOptions> opt = std::make_unique<ProgramOptions>();
    opt->parse(argc, argv);
    auto log_level = opt->getOption<std::string>("log-level");
    EXPECT_STREQ(log_level.c_str(), "debug");

    auto node_type = opt->getNodeType();
    EXPECT_EQ(node_type, NodeType::GATEWAY);
}

TEST(ProgramOptions, CPPStorageOptions) {
    int argc = 1;
    const char* argv[1] = {"epics-k2eg-test"};
    // set environment variable for test
    clearenv();
    setenv("EPICS_k2eg_node-type", "storage", 1);
    std::unique_ptr<ProgramOptions> opt = std::make_unique<ProgramOptions>();
    opt->parse(argc, argv);

    auto node_type = opt->getNodeType();
    EXPECT_EQ(node_type, NodeType::STORAGE);
}

#define VAR_NAME(a, b) a #b
TEST(ProgramOptions, LogConfiguration) {
    int argc = 1;
    const char* argv[1] = {"epics-k2eg-test"};
    // set environment variable for test
    clearenv();
    setenv(std::string("EPICS_k2eg_").append(LOG_ON_CONSOLE).c_str(), "true", 1);
    setenv(std::string("EPICS_k2eg_").append(LOG_ON_FILE).c_str(), "true", 1);
    setenv(std::string("EPICS_k2eg_").append(LOG_FILE_NAME).c_str(), "log-file-name", 1);
    setenv(std::string("EPICS_k2eg_").append(LOG_FILE_MAX_SIZE).c_str(), "1234", 1);
    setenv(std::string("EPICS_k2eg_").append(LOG_ON_SYSLOG).c_str(), "true", 1);
    setenv(std::string("EPICS_k2eg_").append(SYSLOG_SERVER).c_str(), "syslog-server", 1);
    setenv(std::string("EPICS_k2eg_").append(SYSLOG_PORT).c_str(), "5678", 1);
    std::unique_ptr<ProgramOptions> opt = std::make_unique<ProgramOptions>();
    opt->parse(argc, argv);
    auto logger_configuration = opt->getloggerConfiguration();
    EXPECT_EQ(logger_configuration->log_on_console, true);
    EXPECT_EQ(logger_configuration->log_on_file, true);
    EXPECT_STREQ(logger_configuration->log_file_name.c_str(), "log-file-name");
    EXPECT_EQ(logger_configuration->log_file_max_size_mb, 1234);
    EXPECT_EQ(logger_configuration->log_on_syslog, true);
    EXPECT_STREQ(logger_configuration->log_syslog_srv.c_str(), "syslog-server");
    EXPECT_EQ(logger_configuration->log_syslog_srv_port, 5678);
}

TEST(ProgramOptions, FileConfiguration) {
    int argc = 1;
    const char* argv[1] = {"epics-k2eg-test"};
    // set environment variable for test
    const std::string location = fs::path(fs::current_path()) / "test/test-conf-file.conf";
    clearenv();
    setenv("EPICS_k2eg_conf-file", "true", 1);
    setenv("EPICS_k2eg_conf-file-name", location.c_str(), 1);
    std::unique_ptr<ProgramOptions> opt = std::make_unique<ProgramOptions>();
    EXPECT_NO_THROW(opt->parse(argc, argv););
    auto logger_configuration = opt->getloggerConfiguration();
    EXPECT_EQ(logger_configuration->log_on_console, true);
    EXPECT_EQ(logger_configuration->log_file_max_size_mb, 1234);
    EXPECT_EQ(logger_configuration->log_on_syslog, true);
    EXPECT_STREQ(logger_configuration->log_syslog_srv.c_str(), "syslog-server");
    EXPECT_EQ(logger_configuration->log_syslog_srv_port, 5678);
}

TEST(ProgramOptions, FileConfigurationNoPathSpecified) {
    int argc = 1;
    const char* argv[1] = {"epics-k2eg-test"};
    // set environment variable for test
    clearenv();
    setenv("EPICS_k2eg_conf-file", "true", 1);
    setenv("EPICS_k2eg_conf-file-name", "", 1);
    std::unique_ptr<ProgramOptions> opt = std::make_unique<ProgramOptions>();
    ASSERT_THROW(opt->parse(argc, argv), std::runtime_error);
}

TEST(ProgramOptions, FileConfigurationBadPath) {
    int argc = 1;
    const char* argv[1] = {"epics-k2eg-test"};
    // set environment variable for test
    clearenv();
    setenv("EPICS_k2eg_conf-file", "true", 1);
    setenv("EPICS_k2eg_conf-file-name", "/bad/file/path", 1);
    std::unique_ptr<ProgramOptions> opt = std::make_unique<ProgramOptions>();
    ASSERT_THROW(opt->parse(argc, argv), std::runtime_error);
}

TEST(ProgramOptions, PublisherConfiguration) {
    int argc = 1;
    const char* argv[1] = {"epics-k2eg-test"};
    // set environment variable for test
    ConstPublisherConfigurationUPtr conf;
    clearenv();
    setenv("EPICS_k2eg_pub-server-address", "pub-server", 1);
    setenv("EPICS_k2eg_pub-impl-kv", "k1:v1", 1);
    std::unique_ptr<ProgramOptions> opt = std::make_unique<ProgramOptions>();
    EXPECT_NO_THROW(opt->parse(argc, argv););
    EXPECT_NO_THROW(conf = opt->getPublisherConfiguration(););
    EXPECT_STREQ(conf->server_address.c_str(), "pub-server");
    EXPECT_EQ(conf->custom_impl_parameter.size(), 1);
    EXPECT_STREQ(conf->custom_impl_parameter.at("k1").c_str(), "v1");
}

TEST(ProgramOptions, SubscriberConfigurationConfFile) {
    int argc = 1;
    const char* argv[1] = {"epics-k2eg-test"};
    // set environment variable for test
    ConstSubscriberConfigurationUPtr conf;
    const std::string location = fs::path(fs::current_path()) / "test/test-conf-file.conf";
    clearenv();
    setenv("EPICS_k2eg_conf-file", "true", 1);
    setenv("EPICS_k2eg_conf-file-name", location.c_str(), 1);
    std::unique_ptr<ProgramOptions> opt = std::make_unique<ProgramOptions>();
    EXPECT_NO_THROW(opt->parse(argc, argv););
    EXPECT_NO_THROW(conf = opt->getSubscriberConfiguration(););
    EXPECT_STREQ(conf->server_address.c_str(), "sub-address");
    EXPECT_STREQ(conf->group_id.c_str(), "sub-group-id");
    EXPECT_EQ(conf->custom_impl_parameter.size(), 2);
    EXPECT_STREQ(conf->custom_impl_parameter.at("k1").c_str(), "v1");
    EXPECT_STREQ(conf->custom_impl_parameter.at("k2").c_str(), "v2");
}

TEST(ProgramOptions, MetricConfiguration) {
    int argc = 1;
    const char* argv[1] = {"epics-k2eg-test"};
    // set environment variable for test
    clearenv();
    setenv("EPICS_k2eg_metric-server-http-port", "8888", 1);
    std::unique_ptr<ProgramOptions> opt = std::make_unique<ProgramOptions>();
    EXPECT_NO_THROW(opt->parse(argc, argv););
    auto metric_configuration = opt->getMetricConfiguration();
    EXPECT_EQ(metric_configuration->tcp_port, 8888);
}

TEST(ProgramOptions, SchedulerConfiguration) {
    int argc = 1;
    const char* argv[1] = {"epics-k2eg-test"};
    // set environment variable for test
    clearenv();
    setenv("EPICS_k2eg_scheduler-thread-number", "100", 1);
    std::unique_ptr<ProgramOptions> opt = std::make_unique<ProgramOptions>();
    EXPECT_NO_THROW(opt->parse(argc, argv););
    auto scheduler_configuration = opt->getSchedulerConfiguration();
    EXPECT_EQ(scheduler_configuration->thread_number, 100);
}

TEST(ProgramOptions, NodeControllerConfiguration) {
    int argc = 1;
    const char* argv[1] = {"epics-k2eg-test"};
    // set environment variable for test
    clearenv();
    setenv(std::string("EPICS_k2eg_").append(NC_MONITOR_EXPIRATION_TIMEOUT).c_str(), "60", 1);
    setenv(std::string("EPICS_k2eg_").append(NC_MONITOR_PURGE_QUEUE_ON_EXP_TOUT).c_str(), "false", 1);
    setenv(std::string("EPICS_k2eg_").append(NC_MONITOR_CONSUMER_FILTEROUT_REGEX).c_str(), "regex1", 1);
    std::unique_ptr<ProgramOptions> opt = std::make_unique<ProgramOptions>();
    EXPECT_NO_THROW(opt->parse(argc, argv););
    auto node_controller_configuration = opt->getNodeControllerConfiguration();
    EXPECT_EQ(node_controller_configuration->monitor_command_configuration.monitor_checker_configuration.monitor_expiration_timeout, 60);
    EXPECT_EQ(node_controller_configuration->monitor_command_configuration.monitor_checker_configuration.purge_queue_on_monitor_timeout, false);
    EXPECT_EQ(node_controller_configuration->monitor_command_configuration.monitor_checker_configuration.filter_out_regex.size(), 1);
    EXPECT_STREQ(node_controller_configuration->monitor_command_configuration.monitor_checker_configuration.filter_out_regex[0].c_str(), "regex1");
    EXPECT_EQ(node_controller_configuration->snapshot_command_configuration.continuous_snapshot_configuration.snapshot_processing_thread_count, 1);
}

TEST(ProgramOptions, EpicsManagerConfiguration) {
    int argc = 1;
    const char* argv[1] = {"epics-k2eg-test"};
    // set environment variable for test
    clearenv();
    setenv(std::string("EPICS_k2eg_").append(EPICS_MONITOR_THREAD_COUNT).c_str(), "10", 1);
    std::unique_ptr<ProgramOptions> opt = std::make_unique<ProgramOptions>();
    EXPECT_NO_THROW(opt->parse(argc, argv););
    auto epics_manager_configuration = opt->getEpicsManagerConfiguration();
    EXPECT_EQ(epics_manager_configuration->thread_count, 10);
}

TEST(ProgramOptions, ConfigurationServiceConfiguration) {
    int argc = 1;
    const char* argv[1] = {"k2eg-test"};
    // set environment variable for test
    clearenv();
    setenv(std::string("EPICS_k2eg_").append(CONFIGURATION_SERVICE_HOST).c_str(), "consul", 1);
    std::unique_ptr<ProgramOptions> opt = std::make_unique<ProgramOptions>();
    EXPECT_NO_THROW(opt->parse(argc, argv););
    auto conf = opt->getConfigurationServiceConfiguration();
    EXPECT_STREQ(conf->config_server_host.c_str(), "consul");
    EXPECT_EQ(conf->config_server_port, 8500);
}
#endif //__linux__