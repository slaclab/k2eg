
#include <k2eg/service/metric/IMetricService.h>
#include <k2eg/service/metric/impl/prometheus/PrometheusMetricService.h>
#include <k2eg/service/metric/impl/prometheus/PrometheusEpicsMetric.h>
#include <k2eg/service/metric/impl/prometheus/PrometheusCMDControllerMetric.h>
#include <algorithm>
#include <memory>
#include "k2eg/service/metric/INodeControllerMetric.h"
#include "k2eg/service/metric/impl/prometheus/PrometheusNodeControllerMetric.h"

using namespace prometheus;

using namespace k2eg::service::metric;
using namespace k2eg::service::metric::impl::prometheus_impl;

#define INSTANTIATE_METRIC(type, exposer, var) \
auto tmp = std::shared_ptr<type>(new type()); \
var = tmp; \
exposer->RegisterCollectable(tmp->registry);

PrometheusMetricService::PrometheusMetricService(ConstMetricConfigurationUPtr metric_configuration) : IMetricService(std::move(metric_configuration)) {
  std::string uri = "0.0.0.0:" + std::to_string(this->metric_configuration->tcp_port);
  exposer_uptr    = std::make_unique<Exposer>(uri);
}

PrometheusMetricService::~PrometheusMetricService() {}

IEpicsMetric&
PrometheusMetricService::getEpicsMetric() {
  std::lock_guard<std::mutex> lk(service_mux);
  if (!epics_metric) {
    INSTANTIATE_METRIC(PrometheusEpicsMetric, exposer_uptr, epics_metric)
  }
  return *epics_metric;
}

ICMDControllerMetric&
PrometheusMetricService::getCMDControllerMetric() {
  std::lock_guard<std::mutex> lk(service_mux);
  if (!epics_metric) {
    INSTANTIATE_METRIC(PrometheusCMDControllerMetric, exposer_uptr, cmd_controller_metric)
  }
  return *cmd_controller_metric;
}

INodeControllerMetric&
PrometheusMetricService::getNodeControllerMetric() {
  std::lock_guard<std::mutex> lk(service_mux);
  if (!epics_metric) {
    INSTANTIATE_METRIC(PrometheusNodeControllerMetric, exposer_uptr, node_controller_metric)
  }
  return *node_controller_metric;
}