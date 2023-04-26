#ifndef K2EG_SERVICE_METRIC_IMPL_PROMETHEUSMETRICSERVICE_H_
#define K2EG_SERVICE_METRIC_IMPL_PROMETHEUSMETRICSERVICE_H_

#include <k2eg/common/types.h>
#include <k2eg/service/metric/IMetricService.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>

#include <memory>
#include <mutex>
#include <string>
#include "prometheus/counter.h"
#include "prometheus/labels.h"

namespace k2eg::service::metric::impl {

class PrometheusMetricService;

// low level api provider for metric based on prometheus
class PrometheusEpicsMetric : public IEpicsMetric {
  friend class PrometheusMetricService;
  std::shared_ptr<prometheus::Registry> registry;
  prometheus::Family<prometheus::Counter>& ioc_read_write;
  prometheus::Counter& read_counter;
  prometheus::Counter& write_counter;
  prometheus::Counter& monitor_event_data;
  prometheus::Counter& monitor_event_fail;
  prometheus::Counter& monitor_event_cancel;
  prometheus::Counter& monitor_event_disconnected;
  PrometheusEpicsMetric();
 public:
  virtual ~PrometheusEpicsMetric() = default;
  virtual void incrementCounter(IEpicsMetricCounterType type);
};

// Metric services implementation
class PrometheusMetricService : public IMetricService {
  std::mutex                             service_mux;
  std::unique_ptr<prometheus::Exposer>   exposer_uptr;
  std::shared_ptr<PrometheusEpicsMetric> epics_metric;

 public:
  PrometheusMetricService(ConstMetricConfigurationUPtr metric_configuration);
  virtual ~PrometheusMetricService();
  PrometheusMetricService(const PrometheusMetricService&)            = delete;
  PrometheusMetricService& operator=(const PrometheusMetricService&) = delete;

  IEpicsMetric& getEpicsMetric() override final;
};
}  // namespace k2eg::service::metric::impl

#endif  // K2EG_SERVICE_METRIC_IMPL_PROMETHEUSMETRICSERVICE_H_