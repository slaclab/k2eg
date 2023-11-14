#ifndef K2EG_SERVICE_METRIC_IEPICSMETRIC_H_
#define K2EG_SERVICE_METRIC_IEPICSMETRIC_H_

namespace k2eg::service::metric {

// epics counter types
enum class IEpicsMetricCounterType { Get, Put, MonitorTimeout, MonitorData, MonitorFail, MonitorCancel, MonitorDisconnect, TotalMonitor, ActiveMonitor};

// Epics metric group
class IEpicsMetric {
  friend class IMetricService;

 public:
  IEpicsMetric()                                                                      = default;
  virtual ~IEpicsMetric()                                                             = default;
  virtual void incrementCounter(IEpicsMetricCounterType type, double inc_value = 1.0) = 0;
};

}  // namespace k2eg::service::metric

#endif  // K2EG_SERVICE_METRIC_IEPICSMETRIC_H_