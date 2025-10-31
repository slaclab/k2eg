
#include <k2eg/service/metric/IMetricService.h>
#include <k2eg/service/metric/INodeControllerMetric.h>
#include <k2eg/service/metric/impl/prometheus/PrometheusEpicsMetric.h>
#include <k2eg/service/metric/impl/prometheus/PrometheusMetricService.h>
#include <k2eg/service/metric/impl/prometheus/PrometheusCMDControllerMetric.h>
#include <k2eg/service/metric/impl/prometheus/PrometheusNodeControllerMetric.h>
#include <k2eg/service/metric/impl/prometheus/PrometheusNodeControllerSystemMetric.h>
#include <k2eg/service/metric/impl/prometheus/PrometheusStorageNodeMetric.h>

#include <memory>
#include <cassert>


using namespace prometheus;

using namespace k2eg::service::metric;
using namespace k2eg::service::metric::impl::prometheus_impl;

#define INSTANTIATE_METRIC(type, exposer, var) \
auto tmp = std::shared_ptr<type>(new type()); \
var = tmp; \
exposer->RegisterCollectable(tmp->registry);

PrometheusMetricService::PrometheusMetricService(ConstMetricConfigurationShrdPtr metric_configuration) : IMetricService(std::move(metric_configuration)) {
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
  assert(epics_metric);
  return *epics_metric;
}

ICMDControllerMetric&
PrometheusMetricService::getCMDControllerMetric() {
  std::lock_guard<std::mutex> lk(service_mux);
  if (!cmd_controller_metric) {
    INSTANTIATE_METRIC(PrometheusCMDControllerMetric, exposer_uptr, cmd_controller_metric)
  }
  assert(cmd_controller_metric);
  return *cmd_controller_metric;
}

INodeControllerMetric&
PrometheusMetricService::getNodeControllerMetric() {
  std::lock_guard<std::mutex> lk(service_mux);
  if (!node_controller_metric) {
    INSTANTIATE_METRIC(PrometheusNodeControllerMetric, exposer_uptr, node_controller_metric)
  }
  assert(node_controller_metric);
  return *node_controller_metric;
}

INodeControllerSystemMetric&
PrometheusMetricService::getNodeControllerSystemMetric() {
  std::lock_guard<std::mutex> lk(service_mux);
  if (!node_controller_system_metric) {
    INSTANTIATE_METRIC(PrometheusNodeControllerSystemMetric, exposer_uptr, node_controller_system_metric) 
  }
  assert(node_controller_system_metric);
  return *node_controller_system_metric;
}

IStorageNodeMetric&
PrometheusMetricService::getStorageNodeMetric() {
  std::lock_guard<std::mutex> lk(service_mux);
  if (!storage_node_metric) {
    INSTANTIATE_METRIC(PrometheusStorageNodeMetric, exposer_uptr, storage_node_metric)
  }
  assert(storage_node_metric);
  return *storage_node_metric;
}
