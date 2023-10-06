#include <k2eg/common/ProgramOptions.h>
#include <sys/types.h>

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ostream>
#include <string>

#include "k2eg/service/metric/IMetricService.h"
#include "k2eg/service/scheduler/Scheduler.h"

using namespace k2eg::common;
using namespace k2eg::service::log;
using namespace k2eg::controller::command;
using namespace k2eg::service::pubsub;
using namespace k2eg::service::metric;
using namespace k2eg::service::scheduler;

namespace po = boost::program_options;
namespace fs = std::filesystem;

ProgramOptions::ProgramOptions() {
  const std::string actual_path = fs::path(fs::current_path()) / "k2eg.sqlite";
  options.add_options()(HELP, "Produce help information")(VERSION, "Print the application version")(
      CONF_FILE, po::value<bool>()->default_value(false), "Specify if we need to load configuration from file")(
      CONF_FILE_NAME, po::value<std::string>()->default_value(""), "Specify the configuration file")(
      LOG_LEVEL, po::value<std::string>()->default_value("info"), "Specify the log level[trace, debug, info, error, fatal]")(
      LOG_ON_CONSOLE, po::value<bool>()->default_value(true)->zero_tokens(), "Specify when the logger print in console")(
      LOG_ON_FILE, po::value<bool>()->default_value(false)->zero_tokens(), "Specify when the logger print in file")(
      LOG_FILE_NAME, po::value<std::string>(), "Specify the log file path")(
      LOG_FILE_MAX_SIZE, po::value<int>()->default_value(1), "Specify the maximum log file size in mbyte")(
      LOG_ON_SYSLOG, po::value<bool>()->default_value(false)->zero_tokens(), "Specify when the logger print in syslog server")(
      SYSLOG_SERVER, po::value<std::string>(), "Specify syslog hotsname")(SYSLOG_PORT, po::value<int>()->default_value(514), "Specify syslog server port")(
      CMD_INPUT_TOPIC, po::value<std::string>(), "Specify the messages bus queue where the k2eg receive the configuration command")(
      CMD_MAX_FECTH_CMD, po::value<unsigned int>()->default_value(10), "The max number of command fetched per consume operation")(
      CMD_MAX_FETCH_TIME_OUT, po::value<unsigned int>()->default_value(250), "Specify the timeout for waith the command in microseconds")(
      PUB_SERVER_ADDRESS, po::value<std::string>(), "Publisher server address")(
      PUB_IMPL_KV, po::value<std::vector<std::string>>(), "The key:value list for publisher implementation driver")(
      SUB_SERVER_ADDRESS, po::value<std::string>(), "Subscriber server address")(
      SUB_GROUP_ID, po::value<std::string>()->default_value("k2eg-default-group"), "Subscriber group id")(
      SUB_IMPL_KV, po::value<std::vector<std::string>>(), "The key:value list for subscriber implementation driver")(
      STORAGE_PATH, po::value<std::string>()->default_value(actual_path), "The path where the storage files are saved")(
      SCHEDULER_THREAD_NUMBER, po::value<uint>()->default_value(1), "The number of the scheduler worker")(
      METRIC_ENABLE, po::value<bool>()->default_value(false), "Enable metric management")(
      METRIC_HTTP_PORT, po::value<unsigned int>()->default_value(8080), "The port used for publish the http metric server");
}

void
ProgramOptions::parse(int argc, const char* argv[]) {
  try {
    po::store(po::command_line_parser(argc, argv).options(options).allow_unregistered().run(), vm);

    po::store(po::parse_environment(options, "EPICS_k2eg_"), vm);
    po::notify(vm);

    // check if we need to load further option from file
    if (vm[CONF_FILE].as<bool>()) {
      const std::string conf_file_name = vm[CONF_FILE_NAME].as<std::string>();
      if (conf_file_name.empty()) { throw std::runtime_error("configuration file has nott been specifyed"); }
      // load from file
      std::ifstream option_file_stream;
      option_file_stream.open(conf_file_name.c_str(), std::ifstream::in);
      if (!option_file_stream) { throw std::runtime_error("Error opening configuration file"); }

      po::store(po::parse_config_file(option_file_stream, options), vm);
      po::notify(vm);
    }
  } catch (po::too_many_positional_options_error& e) {
    // A positional argument like `opt2=option_value_2` was given
    std::cerr << e.what() << std::endl;
    throw std::runtime_error(e.what());
  } catch (po::error_with_option_name& e) {
    // Another usage error occurred
    std::cerr << e.what() << std::endl;
    throw std::runtime_error(e.what());
  }
}

bool
ProgramOptions::optionConfigure(const std::string& name) {
  return vm.count(name) > 0;
}

#define GET_OPTION(opt, type, def)   optionConfigure(opt) ? getOption<type>(opt) : def
#define GET_OPTION_NO_DEF(opt, type) getOption<type>(opt)

ConstLogConfigurationUPtr
ProgramOptions::getloggerConfiguration() {
  return std::make_unique<const LogConfiguration>(LogConfiguration{.log_level            = GET_OPTION_NO_DEF(LOG_LEVEL, std::string),
                                                                   .log_on_console       = GET_OPTION(LOG_ON_CONSOLE, bool, false),
                                                                   .log_on_file          = GET_OPTION(LOG_ON_FILE, bool, false),
                                                                   .log_file_name        = GET_OPTION(LOG_FILE_NAME, std::string, ""),
                                                                   .log_file_max_size_mb = GET_OPTION(LOG_FILE_MAX_SIZE, int, 1),
                                                                   .log_on_syslog        = GET_OPTION(LOG_ON_SYSLOG, bool, false),
                                                                   .log_syslog_srv       = GET_OPTION(SYSLOG_SERVER, std::string, ""),
                                                                   .log_syslog_srv_port  = GET_OPTION(SYSLOG_PORT, int, 514)});
}

ConstCMDControllerConfigUPtr
ProgramOptions::getCMDControllerConfiguration() {
  return std::make_unique<const CMDControllerConfig>(CMDControllerConfig{
      .topic_in             = GET_OPTION(CMD_INPUT_TOPIC, std::string, ""),
      .max_message_to_fetch = GET_OPTION(CMD_MAX_FECTH_CMD, unsigned int, 250),
      .fetch_time_out       = GET_OPTION(CMD_MAX_FETCH_TIME_OUT, unsigned int, 10),
  });
}

MapStrKV
ProgramOptions::parseKVCustomParam(const std::vector<std::string>& kv_vec) {
  MapStrKV impl_config_map;
  std::for_each(std::begin(kv_vec), std::end(kv_vec), [&impl_config_map = impl_config_map](auto& kv) {
    std::vector<std::string> results;
    boost::algorithm::split(results, kv, boost::is_any_of(":"));
    if (results.size() != 2) { throw std::runtime_error("Bad ky paramter"); }
    impl_config_map.insert(MapStrKVPair(results[0], results[1]));
  });
  return impl_config_map;
}

const std::string
ProgramOptions::getHelpDescription() {
  std::stringstream ss_help;
  ss_help << options;
  return ss_help.str();
}

bool
ProgramOptions::hasOption(const std::string& option) {
  return vm.count(option);
}

ConstPublisherConfigurationUPtr
ProgramOptions::getPublisherConfiguration() {
  return std::make_unique<const PublisherConfiguration>(
      PublisherConfiguration{.server_address        = GET_OPTION(PUB_SERVER_ADDRESS, std::string, ""),
                             .custom_impl_parameter = parseKVCustomParam(GET_OPTION(PUB_IMPL_KV, std::vector<std::string>, std::vector<std::string>()))});
}

ConstSubscriberConfigurationUPtr
ProgramOptions::getSubscriberConfiguration() {
  return std::make_unique<const SubscriberConfiguration>(
      SubscriberConfiguration{.server_address        = GET_OPTION(SUB_SERVER_ADDRESS, std::string, ""),
                              .group_id              = GET_OPTION(SUB_GROUP_ID, std::string, ""),
                              .custom_impl_parameter = parseKVCustomParam(GET_OPTION(SUB_IMPL_KV, std::vector<std::string>, std::vector<std::string>()))});
}

ConstMetricConfigurationUPtr
ProgramOptions::getMetricConfiguration() {
  return std::make_unique<const MetricConfiguration>(
      MetricConfiguration{.enable = GET_OPTION(METRIC_ENABLE, bool, false), .tcp_port = GET_OPTION(METRIC_HTTP_PORT, unsigned int, 8080)});
}

ConstSchedulerConfigurationUPtr
ProgramOptions::getSchedulerConfiguration() {
  return std::make_unique<const SchedulerConfiguration>(
    SchedulerConfiguration{
        .thread_number = GET_OPTION(SCHEDULER_THREAD_NUMBER, unsigned int, 1)
        }
        );
}

const std::string
ProgramOptions::getStoragePath() {
  return GET_OPTION_NO_DEF(STORAGE_PATH, std::string);
}
