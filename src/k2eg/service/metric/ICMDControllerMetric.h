#ifndef K2EG_SERVICE_METRIC_ICMDCONTROLLERMETRIC_H_
#define K2EG_SERVICE_METRIC_ICMDCONTROLLERMETRIC_H_

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
  virtual void incrementCounter(ICMDControllerMetricCounterType type, double inc_value = 1.0) = 0;
};

}  // namespace k2eg::service::metric

#endif  // K2EG_SERVICE_METRIC_ICMDCONTROLLERMETRIC_H_