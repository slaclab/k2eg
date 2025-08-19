#include "k2eg/controller/node/NodeController.h"
#include <cstdlib>
#include <k2eg/common/ProgramOptions.h>
#include <k2eg/k2eg.h>
#include <k2eg/service/storage/StorageServiceFactory.h>
#include <k2eg/service/storage/impl/MongoDBStorageService.h>

#include <memory>
#include <string>


/**
 * Test environment for K2EG
 */
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

/**
 * @brief Create and start a K2EG test environment for unit/integration tests.
 *
 * This helper clears the current process environment and sets a collection of
 * test-specific environment variables appropriate for the requested node
 * type. It then constructs a {@link K2EGTestEnv} instance which calls
 * `K2EG::setup(...)` and, on success, `K2EG::init()`.
 *
 * The returned shared pointer owns the running test environment; when the
 * shared pointer is destroyed the environment's destructor calls
 * `K2EG::deinit()` to shut down services and threads.
 *
 * @param tcp_port  [in,out] A port base used to assign the metrics HTTP port.
 *                  The function increments this value and uses the incremented
 *                  value when setting the corresponding environment variable
 *                  so tests can avoid port collisions.
 * @param type      Node type to start: GATEWAY, STORAGE or FULL. Controls which
 *                  node-specific environment variables are set.
 * @param enable_debug_log  If true, enables console logging and sets the log
 *                  level to debug through environment variables.
 * @param reset_conf If true, sets the configuration-reset-on-start variable so
 *                   the configuration store is reset for a clean test run.
 *
 * @return A non-null std::shared_ptr<K2EGTestEnv> on success. The caller owns
 *         the returned pointer and may call reset() to stop the environment.
 *
 * @throws std::runtime_error if an unknown NodeType is provided.
 *
 * @note This function calls clearenv() and therefore removes all existing
 * environment variables before setting test-specific values. Only use in test
 * processes where this behavior is acceptable.
 *
 * Environment variables set (non-exhaustive):
 *  - EPICS_k2eg_log-on-console
 *  - EPICS_k2eg_log-level
 *  - EPICS_k2eg_configuration-reset-on-start
 *  - EPICS_k2eg_node-type
 *  - EPICS_k2eg_<CMD_INPUT_TOPIC> (gateway/full)
 *  - EPICS_k2eg_<NC_MONITOR_EXPIRATION_TIMEOUT> (gateway/full)
 *  - EPICS_k2eg_<MONGODB_CONNECTION_STRING_KEY> (storage/full)
 *  - EPICS_k2eg_<SCHEDULER_CHECK_EVERY_AMOUNT_OF_SECONDS>
 *  - EPICS_k2eg_<CONFIGURATION_SERVICE_HOST>
 *  - EPICS_k2eg_<METRIC_ENABLE>
 *  - EPICS_k2eg_<METRIC_HTTP_PORT> (uses ++tcp_port)
 *  - EPICS_k2eg_<PUB_SERVER_ADDRESS>
 *  - EPICS_k2eg_<SUB_SERVER_ADDRESS>
 *
 * Example:
 * @code{cpp}
 * int tcp = 9000;
 * auto env = startK2EG(tcp, k2eg::controller::node::NodeType::STORAGE, true, true);
 * // use env for test; env.reset() will stop the environment
 * @endcode
 */
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