#ifndef K2EG_CONTROLLER_NODE_WORKER_MONITOR_MONITORCHECKER_H_
#define K2EG_CONTROLLER_NODE_WORKER_MONITOR_MONITORCHECKER_H_

#include <k2eg/common/types.h>
#include <k2eg/common/broadcaster.h>

#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/pubsub/IPublisher.h>
#include <k2eg/service/scheduler/Scheduler.h>

#include <k2eg/controller/node/configuration/NodeConfiguration.h>

#include <mutex>
#include <regex>
#include <vector>
#include <string>
#include <cstddef>
#include <cstdint>

namespace k2eg::controller::node::worker::monitor {

/**
 * @brief Configuration for MonitorChecker behavior.
 */
struct MonitorCheckerConfiguration {
  /** @brief Seconds of inactivity after which a monitor is considered stale. */
  int64_t monitor_expiration_timeout;
  /** @brief Whether to purge the queue when a monitor times out. */
  bool purge_queue_on_monitor_timeout;
  /** @brief Consumer group name regexes to exclude from activity checks. */
  std::vector<std::string> filter_out_regex;
};

/** @brief Type of monitor lifecycle event. */
enum class MonitorHandlerAction { Start, Stop };

/** @brief Monitor lifecycle event payload. */
typedef struct {
  MonitorHandlerAction                                action;
  k2eg::service::data::repository::ChannelMonitorType monitor_type;
} MonitorHandlerData;

/** @brief Callback invoked on monitor start/stop decisions. */
typedef std::function<void(MonitorHandlerData)> CheckerEventHandler;

struct ChannelMonitorTypeComparator {
  bool
  operator()(const k2eg::service::data::repository::ChannelMonitorType& lhs, const k2eg::service::data::repository::ChannelMonitorType& rhs) const {
    return lhs.id < rhs.id;
  }
};

/**
 * @brief Checks active monitors and emits start/stop events based on activity.
 *
 * Periodically evaluates consumer activity to stop stale monitors and restarts
 * required ones by inspecting the configuration database.
 */
class MonitorChecker {
  const MonitorCheckerConfiguration             monitor_checker_configuration;
  service::pubsub::IPublisherShrdPtr            publisher;
  service::log::ILoggerShrdPtr                  logger;
  configuration::NodeConfigurationShrdPtr       node_configuration_db;
  k2eg::service::scheduler::SchedulerShrdPtr    scheduler;
  k2eg::common::broadcaster<MonitorHandlerData> handler_broadcaster;
  std::vector<std::regex>                       vec_fillout_regex;
  std::mutex                                    op_mux;
  // timeout in second that when expired the queue can be purged and moitor stopped
  int64_t                                                                                     expiration_timeout;
  std::set<k2eg::service::data::repository::ChannelMonitorType, ChannelMonitorTypeComparator> to_stop;
  bool isTimeoutExperid(const k2eg::service::data::repository::ChannelMonitorType& monitor_info);
  bool excludeConsumer(std::string consumer_group_name);
 public:
  /** @brief Construct a MonitorChecker with configuration and node DB. */
  MonitorChecker(const MonitorCheckerConfiguration& monitor_checker_configuration, configuration::NodeConfigurationShrdPtr node_configuration_db);
  ~MonitorChecker();
  /**
   * @brief Register a handler to receive lifecycle events.
   * @return A token that must be held while receiving events.
   */
  k2eg::common::BroadcastToken addHandler(CheckerEventHandler handler);
  /** @brief Persist the latest monitor state snapshot. */
  void                         storeMonitorData(const k2eg::controller::node::configuration::ChannelMonitorTypeConstVector& monitor_info);
  /** @brief Scan a subset of items, stopping monitors that appear idle. */
  size_t                       scanForMonitorToStop(size_t element_to_process = 10);
  /** @brief Scan a subset of items, restarting monitors that should run. */
  size_t                       scanForRestart(size_t element_to_process = 10);
  /** @brief Reset internal lists of monitors to process. */
  void                         resetMonitorToProcess();
  /**
   * @brief Override expiration timeout for active monitors.
   * @param expiration_timeout New timeout in seconds (<0 to restore default).
   */
  void setPurgeTimeout(int64_t expiration_timeout);
};

DEFINE_PTR_TYPES(MonitorChecker);

}  // namespace k2eg::controller::node::worker::monitor

#endif  // K2EG_CONTROLLER_NODE_WORKER_MONITOR_MONITORCHECKER_H_
