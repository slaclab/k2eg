#ifndef __PROGRAMOPTIONS_H__
#define __PROGRAMOPTIONS_H__

#include <boost/program_options.hpp>
#include <k2eg/controller/node/NodeController.h>

#include <k2eg/common/types.h>
#include <k2eg/controller/command/CMDController.h>
#include <k2eg/controller/node/worker/StorageWorker.h>
#include <k2eg/service/configuration/configuration.h>
#include <k2eg/service/epics/EpicsServiceManager.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/metric/IMetricService.h>
#include <k2eg/service/pubsub/IPublisher.h>
#include <k2eg/service/pubsub/ISubscriber.h>
#include <k2eg/service/scheduler/Scheduler.h>
#include <k2eg/service/storage/StorageServiceFactory.h>

namespace po = boost::program_options;

static const char* const HELP = "help";
static const char* const VERSION = "version";

static const char* const CONF_FILE = "conf-file";
static const char* const CONF_FILE_NAME = "conf-file-name";
static const char* const NODE_TYPE = "node-type";
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

// node controller configuration
static const char* const NC_MONITOR_CONSUMER_FILTEROUT_REGEX = "nc-monitor-consumer-filterout-regex";
static const char* const NC_MONITOR_EXPIRATION_TIMEOUT = "nc-monitor-expiration-timeout";
static const char* const NC_MONITOR_PURGE_QUEUE_ON_EXP_TOUT = "nc-purge-queue-on-exp-timeout";

static const char* const PUB_SERVER_ADDRESS = "pub-server-address";
static const char* const PUB_IMPL_KV = "pub-impl-kv";
static const char* const PUB_GROUP_ID = "pub-group-id";
static const char* const PUB_FLUSH_TIMEOUT_MS = "pub-flush-trimeout-ms";

static const char* const SUB_SERVER_ADDRESS = "sub-server-address";
static const char* const SUB_GROUP_ID = "sub-group-id";
static const char* const SUB_IMPL_KV = "sub-impl-kv";

static const char* const STORAGE_PATH = "storage-path";

static const char* const MONITOR_WORKER_SCHEDULE_CRON_CONFIGURATION = "monitor-worker-cron-schedule";
static const char* const SCHEDULER_CHECK_EVERY_AMOUNT_OF_SECONDS = "scheduler-check-delay-seconds";
static const char* const SCHEDULER_THREAD_NUMBER = "scheduler-thread-number";

// snapshot command configuration
static const char* const SNAPSHOT_REPEATING_SCHEDULER_THREAD = "snapshot-repeating-scheduler-thread";

static const char* const METRIC_ENABLE = "metric-enable";
static const char* const METRIC_HTTP_PORT = "metric-server-http-port";

// the number of thread to use for the epics monitor operation (process pv monitor queue in parallel)
static const char* const EPICS_MONITOR_THREAD_COUNT = "epics-monitor-thread-count";
// the maximum number of events to fetch from the single PV monitor queue
static const char* const EPICS_MONITOR_CHANNEL_POLL_MAX = "epics-monitor-channel-poll-max";

static const char* const CONFIGURATION_SERVICE_HOST = "configuration-server-host";
static const char* const CONFIGURATION_SERVICE_PORT = "configuration-server-port";
static const char* const CONFIGURATION_SERVICE_RESET_ON_START = "configuration-reset-on-start";

namespace k2eg {
namespace common {
    /**
     * Options management
     */
    class ProgramOptions
    {
        po::variables_map       vm;
        po::options_description options{"Epics k2eg", 80};

        MapStrKV parseKVCustomParam(const std::vector<std::string>& kv_vec);

        k2eg::controller::node::NodeType node_type_ = k2eg::controller::node::NodeType::GATEWAY;

    public:
        ProgramOptions();
        ~ProgramOptions() = default;
        void parse(int argc, const char* argv[]);

        k2eg::service::log::ConstLogConfigurationUPtr                          getloggerConfiguration();
        k2eg::controller::command::ConstCMDControllerConfigUPtr                getCMDControllerConfiguration();
        k2eg::controller::node::ConstNodeControllerConfigurationUPtr           getNodeControllerConfiguration();
        k2eg::service::pubsub::ConstPublisherConfigurationUPtr                 getPublisherConfiguration();
        k2eg::service::pubsub::ConstSubscriberConfigurationUPtr                getSubscriberConfiguration();
        k2eg::service::metric::ConstMetricConfigurationUPtr                    getMetricConfiguration();
        k2eg::service::scheduler::ConstSchedulerConfigurationUPtr              getSchedulerConfiguration();
        k2eg::service::epics_impl::ConstEpicsServiceManagerConfigUPtr          getEpicsManagerConfiguration();
        k2eg::service::configuration::ConstConfigurationServceiConfigUPtr      getConfigurationServiceConfiguration();
        k2eg::controller::node::worker::ConstStorageWorkerConfigurationShrdPtr getStorageWorkerConfiguration();
        k2eg::service::storage::StorageServiceConfigurationShrdPtr             getStorageServiceConfiguration();
        const std::string                                                      getStoragePath();
        bool                                                                   optionConfigure(const std::string& name);

        template <class T>
        const T& getOption(const std::string& name)
        {
            return vm[name].as<T>();
        }

        k2eg::controller::node::NodeType getNodeType() const;
        bool                             hasOption(const std::string& option);
        const std::string                getHelpDescription();
    };
    DEFINE_PTR_TYPES(ProgramOptions)
} // namespace common

} // namespace k2eg
#endif // __PROGRAMOPTIONS_H__