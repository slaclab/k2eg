#include <k2eg/service/metric/IMetricService.h>
#include <k2eg/service/metric/INodeControllerMetric.h>
#include <k2eg/service/metric/impl/DummyMetricService.h>

#include <prometheus/counter.h>
#include <prometheus/exposer.h>

#include <memory>


using namespace prometheus;

using namespace k2eg::service::metric;
using namespace k2eg::service::metric::impl;

DummyMetricService::DummyMetricService(ConstMetricConfigurationShrdPtr metric_configuration) : IMetricService(std::move(metric_configuration)) {}

DummyMetricService::~DummyMetricService() {}

IEpicsMetric&
DummyMetricService::getEpicsMetric() {
  std::lock_guard<std::mutex> lk(service_mux);
  if (!epics_metric) { epics_metric = std::shared_ptr<DummyIEpicsMetric>(new DummyIEpicsMetric()); }
  return *epics_metric;
}

ICMDControllerMetric&
DummyMetricService::getCMDControllerMetric() {
  std::lock_guard<std::mutex> lk(service_mux);
  if (!cmd_controller_metric) { cmd_controller_metric = std::shared_ptr<DummyICMDControllerMetric>(new DummyICMDControllerMetric()); }
  return *cmd_controller_metric;
}

INodeControllerMetric&
DummyMetricService::getNodeControllerMetric() {
  std::lock_guard<std::mutex> lk(service_mux);
  if (!node_controller_metric) { node_controller_metric = std::shared_ptr<DummyINodeControllerMetric>(new DummyINodeControllerMetric()); }
  return *node_controller_metric;
}

INodeControllerSystemMetric& 
DummyMetricService::getNodeControllerSystemMetric(){
  std::lock_guard<std::mutex> lk(service_mux);
  if (!node_controller_system_metric) {
    node_controller_system_metric = std::shared_ptr<DummyINodeControllerSystemMetric>(new DummyINodeControllerSystemMetric());
  }
  return *node_controller_system_metric;
}
