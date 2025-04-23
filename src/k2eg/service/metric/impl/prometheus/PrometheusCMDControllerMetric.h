#ifndef K2EG_SERVICE_METRIC_IMPL_PROMETHEUS_PROMETHEUSCMDCOMMANDMETRIC_H_
#define K2EG_SERVICE_METRIC_IMPL_PROMETHEUS_PROMETHEUSCMDCOMMANDMETRIC_H_

#include <k2eg/common/types.h>
#include <k2eg/service/metric/ICMDControllerMetric.h>
#include <k2eg/service/metric/impl/prometheus/PrometheusEpicsMetric.h>
#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/labels.h>
#include <prometheus/registry.h>

#include <memory>

namespace k2eg::service::metric::impl::prometheus_impl {

class PrometheusMetricService;

// low level api provider for metric based on prometheus
class PrometheusCMDControllerMetric : public ICMDControllerMetric {
  friend class PrometheusMetricService;
  std::shared_ptr<prometheus::Registry>    registry;
  prometheus::Family<prometheus::Counter>& command_received_counters;
  prometheus::Counter&                     received;
  prometheus::Counter&                     bad_parsed;
  prometheus::Counter&                     fault_processed;
  PrometheusCMDControllerMetric();

 public:
  virtual ~PrometheusCMDControllerMetric() = default;
  void incrementCounter(ICMDControllerMetricCounterType type, const double inc_value = 1.0, const std::map<std::string, std::string>& label = {}) override final;
};
}  // namespace k2eg::service::metric::impl::prometheus_impl

#endif  // K2EG_SERVICE_METRIC_IMPL_PROMETHEUS_PROMETHEUSCMDCOMMANDMETRIC_H_