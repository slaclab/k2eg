#ifndef K2EG_SERVICE_METRIC_IMPL_PROMETHEUS_PROMETHEUSSTORAGENODEMETRIC_H_
#define K2EG_SERVICE_METRIC_IMPL_PROMETHEUS_PROMETHEUSSTORAGENODEMETRIC_H_

#include <k2eg/service/metric/IStorageNodeMetric.h>

#include <prometheus/gauge.h>
#include <prometheus/counter.h>
#include <prometheus/registry.h>

#include <memory>

namespace k2eg::service::metric::impl::prometheus_impl {

class PrometheusMetricService;

/**
 * @brief Prometheus implementation for storage node metrics
 */
class PrometheusStorageNodeMetric : public IStorageNodeMetric {
  friend class PrometheusMetricService;

  std::shared_ptr<prometheus::Registry> registry;
  prometheus::Family<prometheus::Gauge>&   running_archivers_gauge_family;
  prometheus::Family<prometheus::Counter>& pv_recorded_counter_family;
  prometheus::Family<prometheus::Counter>& snapshot_recorded_counter_family;

  PrometheusStorageNodeMetric();

 public:
  virtual ~PrometheusStorageNodeMetric() = default;
  void incrementCounter(IStorageNodeMetricType type,
                        const double inc_value = 1.0,
                        const std::map<std::string, std::string>& label = {}) override final;
};

}  // namespace k2eg::service::metric::impl::prometheus_impl

#endif  // K2EG_SERVICE_METRIC_IMPL_PROMETHEUS_PROMETHEUSSTORAGENODEMETRIC_H_
