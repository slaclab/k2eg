#ifndef __PROGRAMOPTIONS_H__
#define __PROGRAMOPTIONS_H__

#include <boost/program_options.hpp>

#include <k2eg/common/types.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/controller/command/CMDController.h>
#include <k2eg/service/pubsub/IPublisher.h>
#include <k2eg/service/pubsub/ISubscriber.h>

namespace po = boost::program_options;

static const char* const HELP = "help";
static const char* const VERSION = "version";

static const char* const CONF_FILE = "conf-file";
static const char* const CONF_FILE_NAME = "conf-file-name";
static const char* const LOG_LEVEL = "log-level";
static const char* const LOG_ON_CONSOLE = "log-on-console";
static const char* const LOG_ON_FILE = "log-on-file";
static const char* const LOG_FILE_NAME = "log-file-name";
static const char* const LOG_FILE_MAX_SIZE = "log-file-max-size";
static const char* const LOG_ON_SYSLOG = "log-on-syslog";
static const char* const SYSLOG_SERVER = "syslog-server";
static const char* const SYSLOG_PORT = "syslog-port";

static const char* const CMD_INPUT_TOPIC = "cmd-input-topic";
static const char* const CMD_MAX_FECTH_CMD = "cmd-max-fecth-element";
static const char* const CMD_MAX_FETCH_TIME_OUT = "cmd-max-fecth-time-out";

static const char* const PUB_SERVER_ADDRESS = "pub-server-address";
static const char* const PUB_IMPL_KV = "pub-impl-kv";

static const char* const SUB_SERVER_ADDRESS = "sub-server-address";
static const char* const SUB_GROUP_ID = "sub-group-id";
static const char* const SUB_IMPL_KV = "sub-impl-kv";

static const char* const STORAGE_PATH = "storage-path";

namespace k2eg
{
    namespace common
    {
        /**
         * Options management
         */
        class ProgramOptions
        {
            po::variables_map vm;
            po::options_description options{"Epics k2eg", 80};

            MapStrKV parseKVCustomParam(const std::vector<std::string> &kv_vec);
        public:
            ProgramOptions();
            ~ProgramOptions() = default;
            void parse(int argc, const char *argv[]);

            k2eg::service::log::ConstLogConfigurationUPtr getloggerConfiguration();
            k2eg::controller::command::ConstCMDControllerConfigUPtr getCMDControllerConfiguration();
            k2eg::service::pubsub::ConstPublisherConfigurationUPtr getPublisherConfiguration();
            k2eg::service::pubsub::ConstSubscriberConfigurationUPtr getSubscriberConfiguration();
            const std::string getStoragePath();
            bool optionConfigure(const std::string &name);

            template <class T>
            const T &getOption(const std::string &name)
            {
                return vm[name].as<T>();
            }

            bool hasOption(const std::string& option);
            const std::string getHelpDescription();
        };
        DEFINE_PTR_TYPES(ProgramOptions)
    } // namespace common

} // namespace k2eg

#endif // __PROGRAMOPTIONS_H__