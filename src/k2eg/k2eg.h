#ifndef k2eg_H_
#define k2eg_H_

#include <k2eg/common/ProgramOptions.h>
#include <k2eg/controller/command/CMDCommand.h>
#include <k2eg/controller/node/NodeController.h>
#include <k2eg/service/log/ILogger.h>

#include <condition_variable>
#include <mutex>
#include <string>

namespace k2eg {

/**
 * @class K2EG
 * @brief Core application orchestrator for K2EG nodes.
 *
 * The K2EG class is responsible for:
 *   - Parsing command line options and configuration.
 *   - Initializing logging, metrics, command controllers, and the node controller.
 *   - Running the main event loop and dispatching commands to workers.
 *   - Handling shutdown signals and coordinating a graceful teardown.
 */
class K2EG
{
    bool                    quit;
    bool                    running;
    bool                    terminated;
    std::mutex              m;
    std::condition_variable cv;

    std::shared_ptr<k2eg::service::log::ILogger> logger;

    /**
     * @brief Get the instance of the metric service.
     * @param metric_conf The configuration for the metric service.
     * @return A shared pointer to the metric service instance.
     */
    k2eg::service::metric::IMetricServiceShrdPtr instanceMetricService(k2eg::service::metric::ConstMetricConfigurationUPtr metric_conf);

    /**
     * @brief Get the version string of the K2EG application.
     * @param long_version If true, returns a detailed version string with dependencies.
     * @return A string containing the version information.
     */
    std::string getTextVersion(bool long_version = false) const;

protected:
    k2eg::common::ProgramOptionsUPtr             po;              /// Program options manager
    k2eg::controller::command::CMDControllerUPtr cmd_controller;  /// Command controller
    k2eg::controller::node::NodeControllerUPtr   node_controller; /// Node controller
    /**
     * @brief Set up the K2EG orchestrator program option.
     * @param argc Argument count from main().
     * @param argv Argument vector from main().
     * @details This will parse command line options and configuration files.
     * @return True if the app should continue running, false otherwise (has been requested to show help or information).
     */
    bool setup(int argc, const char* argv[]);
    /**
     * @brief Initialize the K2EG orchestrator.
     *
     * This will set up logging, command controllers, and the node controller.
     * It will also parse command line options and configuration files.
     */
    void init();

    /**
     * @brief Deinitialize the K2EG orchestrator.
     *
     * This will shut down all subsystems and release resources.
     */
    void deinit();

public:
    /**
     * @brief Construct and initialize the K2EG orchestrator.
     */
    K2EG();

    /**
     * @brief Destroy the orchestrator, ensuring all subsystems are shut down.
     */
    ~K2EG();

    /**
     * @brief Start the application and enter the main loop.
     *
     * This will initialize all subsystems, then block until stop() is called.
     *
     * @param argc Argument count from main().
     * @param argv Argument vector from main().
     * @return Exit code (0 on success, non-zero on error).
     */
    int run(int argc, const char* argv[]);

    /**
     * @brief Request a graceful shutdown of the application.
     *
     * This will cause run() to exit and begin resource cleanup.
     */
    void stop();

    /**
     * @brief Check if a stop() request has been received.
     * @return True if stop() was called, false otherwise.
     */
    bool isStopRequested() const
    {
        return quit;
    }

    /**
     * @brief Check if the application is currently running.
     * @return True if run() is active, false otherwise.
     */
    bool isRunning() const
    {
        return running;
    }

    /**
     * @brief Check if the application has completed shutdown.
     * @return True if teardown has finished, false otherwise.
     */
    bool isTerminated() const
    {
        return terminated;
    }
};

} // namespace k2eg

#endif // k2eg_H_