#ifndef K2EG_SERVICE_METRIC_IMPL_PROMETHEUS_PROMETHEUSEPICSMETRIC_H_
#define K2EG_SERVICE_METRIC_IMPL_PROMETHEUS_PROMETHEUSEPICSMETRIC_H_

#include <k2eg/service/metric/IEpicsMetric.h>

#include <prometheus/counter.h>
#include <prometheus/labels.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <memory>

namespace k2eg::service::metric::impl::prometheus_impl {

class PrometheusMetricService;

// low level api provider for metric based on prometheus
class PrometheusEpicsMetric : public IEpicsMetric {
  friend class PrometheusMetricService;
  std::shared_ptr<prometheus::Registry>    registry;
  prometheus::Family<prometheus::Counter>& ioc_read_write;
  prometheus::Counter&                     get_ok_counter;
  prometheus::Counter&                     put_ok_counter;
  prometheus::Counter&                     monitor_event_data;
  prometheus::Counter&                     monitor_event_fail;
  prometheus::Counter&                     monitor_event_cancel;
  prometheus::Counter&                     monitor_event_disconnected;
  PrometheusEpicsMetric();

 public:
  virtual ~PrometheusEpicsMetric() = default;
  void incrementCounter(IEpicsMetricCounterType type, double inc_value = 1.0) override final;
};
}  // namespace k2eg::service::metric::impl::prometheus_impl

#endif  // K2EG_SERVICE_METRIC_IMPL_PROMETHEUS_PROMETHEUSEPICSMETRIC_H_
