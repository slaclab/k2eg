#ifndef K2EG_CONTROLLER_NODE_WORKER_MONITOR_MONITORCHECKER_H_
#define K2EG_CONTROLLER_NODE_WORKER_MONITOR_MONITORCHECKER_H_

#include <k2eg/common/broadcaster.h>

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>
#include <string>
#include <regex>

#include "k2eg/common/types.h"
#include "k2eg/controller/node/configuration/NodeConfiguration.h"
#include "k2eg/service/log/ILogger.h"
#include "k2eg/service/pubsub/IPublisher.h"
#include "k2eg/service/scheduler/Scheduler.h"

namespace k2eg::controller::node::worker::monitor {

struct MonitorCheckerConfiguration {
  // represent the seconds timeout where a monitor queue is  not conusmed
  // ater these second are expired, the monitor that push on the specific
  // queue is stopped
  int64_t monitor_expiration_timeout;
  // if true the queue is pruged when monitor is stopped
  bool purge_queue_on_monitor_timeout;

  // regex to filterout consumer for which used to count
  // when a queue is used or not
  std::vector<std::string> filter_out_regex;
};

// the type of the monitor events
enum class MonitorHandlerAction { Start, Stop };

// the struct that describe the monitor event
typedef struct {
  MonitorHandlerAction                                action;
  k2eg::service::data::repository::ChannelMonitorType monitor_type;
} MonitorHandlerData;

// handler to call when a new monitor is needed (PV Name, protocol, destination topic)
typedef std::function<void(MonitorHandlerData)> CheckerEventHandler;

struct ChannelMonitorTypeComparator {
  bool
  operator()(const k2eg::service::data::repository::ChannelMonitorType& lhs, const k2eg::service::data::repository::ChannelMonitorType& rhs) const {
    return lhs.id < rhs.id;
  }
};

// class that manage the checking of the activated monitor, when some criteria will
// encountered the monitor will be stopped, the checker inform the parent class for
// start and stop monitor needs according to the above rules. At each restart the
// database is check to see if there are monitor to be started
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
  MonitorChecker(const MonitorCheckerConfiguration& monitor_checker_configuration, configuration::NodeConfigurationShrdPtr node_configuration_db);
  ~MonitorChecker();
  /**
  the returned token needs to be maintaned until event are neede
  */
  k2eg::common::BroadcastToken addHandler(CheckerEventHandler handler);
  void                         storeMonitorData(const k2eg::controller::node::configuration::ChannelMonitorTypeConstVector& monitor_info);
  // scann element for automatic disable the monitoring
  size_t                       scanForMonitorToStop(size_t element_to_process = 10);
  // scann element for restart the monitor
  size_t                       scanForRestart(size_t element_to_process = 10);
  void                         resetMonitorToProcess();
  /**
  Update the configured value for the expiration timeout of the active monitor
  if expiration_timeout is < 0 the default configured value will be restored
  */
  void setPurgeTimeout(int64_t expiration_timeout);
};

DEFINE_PTR_TYPES(MonitorChecker);

}  // namespace k2eg::controller::node::worker::monitor

#endif  // K2EG_CONTROLLER_NODE_WORKER_MONITOR_MONITORCHECKER_H_