#ifndef K2EG_SERVICE_METRIC_IMPL_PROMETHEUSMETRICSERVICE_H_
#define K2EG_SERVICE_METRIC_IMPL_PROMETHEUSMETRICSERVICE_H_

#include <k2eg/common/types.h>
#include <k2eg/service/metric/IMetricService.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>

#include <memory>
#include <mutex>
#include <string>

#include "k2eg/service/metric/ICMDControllerMetric.h"
#include "k2eg/service/metric/IEpicsMetric.h"
#include "k2eg/service/metric/impl/prometheus/PrometheusCMDControllerMetric.h"

namespace k2eg::service::metric::impl::prometheus_impl {

// Metric services implementation
class PrometheusMetricService : public IMetricService {
  std::mutex                            service_mux;
  std::unique_ptr<prometheus::Exposer>  exposer_uptr;
  std::shared_ptr<IEpicsMetric>         epics_metric;
  std::shared_ptr<ICMDControllerMetric> cmd_controller_metric;

 public:
  PrometheusMetricService(ConstMetricConfigurationUPtr metric_configuration);
  virtual ~PrometheusMetricService();
  PrometheusMetricService(const PrometheusMetricService&)            = delete;
  PrometheusMetricService& operator=(const PrometheusMetricService&) = delete;

  IEpicsMetric&         getEpicsMetric() override final;
  ICMDControllerMetric& getCMDControllerMetric() override final;
};
}  // namespace k2eg::service::metric::impl::prometheus_impl

#endif  // K2EG_SERVICE_METRIC_IMPL_PROMETHEUSMETRICSERVICE_H_