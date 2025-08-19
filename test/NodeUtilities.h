#include "k2eg/controller/node/NodeController.h"
#include <cstdlib>
#include <k2eg/common/ProgramOptions.h>
#include <k2eg/k2eg.h>
#include <k2eg/service/storage/StorageServiceFactory.h>
#include <k2eg/service/storage/impl/MongoDBStorageService.h>

#include <memory>
#include <string>

class K2EGTestEnv : public k2eg::K2EG
{
public:
    K2EGTestEnv()
    {
        int         argc = 1;
        const char* argv[1] = {"epics-k2eg-test"};
        if (K2EG::setup(argc, argv))
        {
            init();
        }
    }

    ~K2EGTestEnv()
    {
        deinit();
    }
};

inline std::shared_ptr<K2EGTestEnv> startK2EG(int& tcp_port, k2eg::controller::node::NodeType type, bool enable_debug_log = false, bool reset_conf = true)
{
    clearenv();
    if (enable_debug_log)
    {
        setenv("EPICS_k2eg_log-on-console", "true", 1);
        setenv("EPICS_k2eg_log-level", "debug", 1);
        setenv(("EPICS_k2eg_" + std::string(LOG_DEBUG_INFO)).c_str(), "true", 1);
    }
    else
    {
        setenv("EPICS_k2eg_log-on-console", "false", 1);
    }

    if (reset_conf)
    {
        setenv("EPICS_k2eg_configuration-reset-on-start", "true", 1);
    }

    switch (type)
    {
    case k2eg::controller::node::NodeType::GATEWAY:
        setenv("EPICS_k2eg_node-type", "gateway", 1);
        setenv(("EPICS_k2eg_" + std::string(CMD_INPUT_TOPIC)).c_str(), "cmd-in-topic", 1);
        setenv(("EPICS_k2eg_" + std::string(NC_MONITOR_EXPIRATION_TIMEOUT)).c_str(), "1", 1);
        break;
    case k2eg::controller::node::NodeType::STORAGE:
        setenv("EPICS_k2eg_node-type", "storage", 1);
        setenv(("EPICS_k2eg_" + std::string(k2eg::service::storage::impl::MONGODB_CONNECTION_STRING_KEY)).c_str(), "mongodb://admin:admin@mongodb-primary:27017", 1);
        break;
    case k2eg::controller::node::NodeType::FULL:
        setenv("EPICS_k2eg_node-type", "full", 1);
        setenv(("EPICS_k2eg_" + std::string(CMD_INPUT_TOPIC)).c_str(), "cmd-in-topic", 1);
        setenv(("EPICS_k2eg_" + std::string(NC_MONITOR_EXPIRATION_TIMEOUT)).c_str(), "1", 1);
        setenv(("EPICS_k2eg_" + std::string(k2eg::service::storage::impl::MONGODB_CONNECTION_STRING_KEY)).c_str(), "mongodb://admin:admin@mongodb-primary:27017", 1);
        break;
    default:
        throw std::runtime_error("Unknown node type");
    }
    setenv(("EPICS_k2eg_" + std::string(SCHEDULER_CHECK_EVERY_AMOUNT_OF_SECONDS)).c_str(), "1", 1);
    // set monitor expiration time out at minimum
    setenv(("EPICS_k2eg_" + std::string(CONFIGURATION_SERVICE_HOST)).c_str(), "consul", 1);
    setenv(("EPICS_k2eg_" + std::string(METRIC_ENABLE)).c_str(), "true", 1);
    setenv(("EPICS_k2eg_" + std::string(METRIC_HTTP_PORT)).c_str(), std::to_string(++tcp_port).c_str(), 1);
    setenv(("EPICS_k2eg_" + std::string(PUB_SERVER_ADDRESS)).c_str(), "kafka:9092", 1);
    setenv(("EPICS_k2eg_" + std::string(SUB_SERVER_ADDRESS)).c_str(), "kafka:9092", 1);
    return std::make_shared<K2EGTestEnv>();
}