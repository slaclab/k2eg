#ifndef K2EG_SERVICE_METRIC_ICMDCONTROLLERMETRIC_H_
#define K2EG_SERVICE_METRIC_ICMDCONTROLLERMETRIC_H_

#include <map>
#include <string>

namespace k2eg::service::metric {
class IMetricService;
// epics counter types
enum class ICMDControllerMetricCounterType {ReceivedCommand, BadCommand, FaultProcessingCommand};

// Epics metric group
class ICMDControllerMetric {
  friend class IMetricService;

 public:
  ICMDControllerMetric()                                                                       = default;
  virtual ~ICMDControllerMetric()                                                              = default;
  virtual void incrementCounter(ICMDControllerMetricCounterType type, const double inc_value = 1.0, const std::map<std::string, std::string>& label = {}) = 0;
};

}  // namespace k2eg::service::metric

#endif  // K2EG_SERVICE_METRIC_ICMDCONTROLLERMETRIC_H_