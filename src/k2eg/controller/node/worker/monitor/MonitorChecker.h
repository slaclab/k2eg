#ifndef K2EG_CONTROLLER_NODE_WORKER_MONITOR_MONITORCHECKER_H_
#define K2EG_CONTROLLER_NODE_WORKER_MONITOR_MONITORCHECKER_H_

#include "k2eg/common/types.h"
#include "k2eg/controller/node/configuration/NodeConfiguration.h"
#include "k2eg/service/pubsub/IPublisher.h"
#include "k2eg/service/scheduler/Scheduler.h"
#include <k2eg/common/broadcaster.h>

#include <mutex>

namespace k2eg::controller::node::worker::monitor {

enum class MonitorHandlerAction{
    Start,
    Stop
} ;

typedef struct {
    MonitorHandlerAction action;
    k2eg::service::data::repository::ChannelMonitorType monitor_type;
} MonitorHandlerData;

// handler to call when a new monitor is needed (PV Name, protocol, destination topic)
typedef std::function<void(MonitorHandlerData)> CheckerEventHandler;

// class that manage the checking of the activated monitor, when some criteria will
// encountered the monitor will be stopped, the checker inform the parent class for 
// start and stop monitor needs according to the above rules. At each restart the 
// database is check to see if there are monitor to be started
class MonitorChecker {
  service::pubsub::IPublisherShrdPtr         publisher;
  configuration::NodeConfigurationShrdPtr    node_configuration_db;
  k2eg::service::scheduler::SchedulerShrdPtr scheduler;
  k2eg::common::broadcaster<MonitorHandlerData> handler_broadcaster;
  std::mutex op_mux;
 public:
  MonitorChecker(service::pubsub::IPublisherShrdPtr publisher, configuration::NodeConfigurationShrdPtr node_configuration_db);
  ~MonitorChecker();
  /**
  the returned token needs to be maintaned until event are neede
  */
  k2eg::common::BroadcastToken addHandler(CheckerEventHandler handler);
  void storeMonitorData(const k2eg::controller::node::configuration::ChannelMonitorTypeConstVector& monitor_info);
};

DEFINE_PTR_TYPES(MonitorChecker);

}  // namespace k2eg::controller::node::worker::monitor

#endif  // K2EG_CONTROLLER_NODE_WORKER_MONITOR_MONITORCHECKER_H_