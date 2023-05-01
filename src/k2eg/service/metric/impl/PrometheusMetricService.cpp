
#include <k2eg/service/metric/IMetricService.h>
#include <k2eg/service/metric/impl/PrometheusMetricService.h>
#include <prometheus/counter.h>
#include <prometheus/exposer.h>

#include <algorithm>
#include <memory>

using namespace prometheus;

using namespace k2eg::service::metric;
using namespace k2eg::service::metric::impl;

PrometheusEpicsMetric::PrometheusEpicsMetric()
    : registry(std::make_shared<Registry>()),
      ioc_read_write(BuildCounter().Name("epics_ioc_operation").Help("Epics description").Register(*registry)),
      get_ok_counter(ioc_read_write.Add({{"op", "get"}})),
      put_ok_counter(ioc_read_write.Add({{"op", "put"}})),
      monitor_event_data(ioc_read_write.Add({{"op", "monitor"}, {"evt_type", "data"}})),
      monitor_event_fail(ioc_read_write.Add({{"op", "monitor"}, {"evt_type", "fail"}})),
      monitor_event_cancel(ioc_read_write.Add({{"op", "monitor"}, {"evt_type", "cancel"}})),
      monitor_event_disconnected(ioc_read_write.Add({{"op", "monitor"}, {"evt_type", "disconnect"}})) {}

void PrometheusEpicsMetric::incrementCounter(IEpicsMetricCounterType type, double inc_value) {
  switch (type) {
    case IEpicsMetricCounterType::Get: get_ok_counter.Increment(inc_value); break;
    case IEpicsMetricCounterType::Put: put_ok_counter.Increment(inc_value); break;
    case IEpicsMetricCounterType::MonitorData: monitor_event_data.Increment(inc_value); break;
    case IEpicsMetricCounterType::MonitorFail: monitor_event_fail.Increment(inc_value); break;
    case IEpicsMetricCounterType::MonitorCancel: monitor_event_cancel.Increment(inc_value); break;
    case IEpicsMetricCounterType::MonitorDisconnect: monitor_event_disconnected.Increment(inc_value); break;
    case IEpicsMetricCounterType::MonitorTimeout: break;
  }
}

PrometheusMetricService::PrometheusMetricService(ConstMetricConfigurationUPtr metric_configuration) : IMetricService(std::move(metric_configuration)) {
  std::string uri = "0.0.0.0:" + std::to_string(this->metric_configuration->tcp_port);
  exposer_uptr    = std::make_unique<Exposer>(uri);
}

PrometheusMetricService::~PrometheusMetricService() {}

IEpicsMetric&
PrometheusMetricService::getEpicsMetric() {
  std::lock_guard<std::mutex> lk(service_mux);
  if (!epics_metric) { 
    epics_metric = std::shared_ptr<PrometheusEpicsMetric>(new PrometheusEpicsMetric()); 
    exposer_uptr->RegisterCollectable(epics_metric->registry);
  }
  return *epics_metric;
}