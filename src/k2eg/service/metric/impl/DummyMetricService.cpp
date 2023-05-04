
#include <k2eg/service/metric/IMetricService.h>
#include <k2eg/service/metric/impl/DummyMetricService.h>
#include <prometheus/counter.h>
#include <prometheus/exposer.h>

#include <algorithm>
#include <memory>

using namespace prometheus;

using namespace k2eg::service::metric;
using namespace k2eg::service::metric::impl;

void
DummyEpicsMetric::incrementCounter(IEpicsMetricCounterType type, double inc_value) {}

void
DummyCMDControllerMetric::incrementCounter(ICMDControllerMetricCounterType type, double inc_value) {}

DummyMetricService::DummyMetricService(ConstMetricConfigurationUPtr metric_configuration) : IMetricService(std::move(metric_configuration)) {}

DummyMetricService::~DummyMetricService() {}

IEpicsMetric&
DummyMetricService::getEpicsMetric() {
  std::lock_guard<std::mutex> lk(service_mux);
  if (!epics_metric) { epics_metric = std::shared_ptr<DummyEpicsMetric>(new DummyEpicsMetric()); }
  return *epics_metric;
}

ICMDControllerMetric&
DummyMetricService::getCMDControllerMetric() {
  std::lock_guard<std::mutex> lk(service_mux);
  if (!cmd_controller_metric) { cmd_controller_metric = std::shared_ptr<DummyCMDControllerMetric>(new DummyCMDControllerMetric()); }
  return *cmd_controller_metric;
}