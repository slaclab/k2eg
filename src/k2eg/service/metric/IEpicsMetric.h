#ifndef K2EG_SERVICE_METRIC_IEPICSMETRIC_H_
#define K2EG_SERVICE_METRIC_IEPICSMETRIC_H_

#include <map>
#include <string>
namespace k2eg::service::metric {

// epics counter types
enum class IEpicsMetricCounterType { Get, Put, MonitorTimeout, MonitorData, MonitorFail, MonitorCancel, MonitorDisconnect, TotalMonitor, ActiveMonitor, ThrottlingIdleCounter, ThrottlingEventCounter, ThrottlingDurationCounter, ThrottleGauge};

// Epics metric group
class IEpicsMetric {
  friend class IMetricService;

 public:
  IEpicsMetric()                                                                      = default;
  virtual ~IEpicsMetric()                                                             = default;
  virtual void incrementCounter(IEpicsMetricCounterType type, const double inc_value = 1.0, const std::map<std::string, std::string>& label = {}) = 0;
};

}  // namespace k2eg::service::metric

#endif  // K2EG_SERVICE_METRIC_IEPICSMETRIC_H_