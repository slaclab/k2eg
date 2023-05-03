
#include <k2eg/service/metric/IMetricService.h>
#include <k2eg/service/metric/impl/prometheus/PrometheusMetricService.h>
#include <k2eg/service/metric/impl/prometheus/PrometheusEpicsMetric.h>
#include <k2eg/service/metric/impl/prometheus/PrometheusCMDControllerMetric.h>
#include <algorithm>
#include <memory>

using namespace prometheus;

using namespace k2eg::service::metric;
using namespace k2eg::service::metric::impl::prometheus_impl;

PrometheusMetricService::PrometheusMetricService(ConstMetricConfigurationUPtr metric_configuration) : IMetricService(std::move(metric_configuration)) {
  std::string uri = "0.0.0.0:" + std::to_string(this->metric_configuration->tcp_port);
  exposer_uptr    = std::make_unique<Exposer>(uri);
}

PrometheusMetricService::~PrometheusMetricService() {}

IEpicsMetric&
PrometheusMetricService::getEpicsMetric() {
  std::lock_guard<std::mutex> lk(service_mux);
  if (!epics_metric) {
    std::shared_ptr<PrometheusEpicsMetric> tmp = std::shared_ptr<PrometheusEpicsMetric>(new PrometheusEpicsMetric());
    epics_metric = tmp;
    exposer_uptr->RegisterCollectable(tmp->registry);
  }
  return *epics_metric;
}

ICMDControllerMetric&
PrometheusMetricService::getCMDControllerMetric() {
  std::lock_guard<std::mutex> lk(service_mux);
  if (!epics_metric) {
    std::shared_ptr<PrometheusCMDControllerMetric> tmp = std::shared_ptr<PrometheusCMDControllerMetric>(new PrometheusCMDControllerMetric());
    cmd_controller_metric = tmp;
    exposer_uptr->RegisterCollectable(tmp->registry);
  }
  return *cmd_controller_metric;
}