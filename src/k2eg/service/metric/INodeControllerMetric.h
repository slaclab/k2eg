#ifndef K2EG_SERVICE_METRIC_INODECONTROLLERMETRIC_H_
#define K2EG_SERVICE_METRIC_INODECONTROLLERMETRIC_H_

#include <map>
#include <string>
namespace k2eg::service::metric {

// epics counter types
enum class INodeControllerMetricCounterType { 
    SubmittedCommand,
    SnapshotEventCounter,
    SnapshotThrottleGauge
    };

// Epics metric group
class INodeControllerMetric {
  friend class IMetricService;

 public:
  INodeControllerMetric()                                                                      = default;
  virtual ~INodeControllerMetric()                                                             = default;
  virtual void incrementCounter(INodeControllerMetricCounterType type, const double inc_value = 1.0, const std::map<std::string, std::string>& label = {}) = 0;
};

}  // namespace k2eg::service::metric

#endif  // K2EG_SERVICE_METRIC_INODECONTROLLERMETRIC_H_