#include <k2eg/service/metric/impl/prometheus/PrometheusEpicsMetric.h>

using namespace prometheus;

using namespace k2eg::service::metric;
using namespace k2eg::service::metric::impl::prometheus_impl;


PrometheusEpicsMetric::PrometheusEpicsMetric()
    : registry(std::make_shared<Registry>()),
      ioc_read_write(BuildCounter().Name("k2eg_epics_ioc_operation").Help("Metric set for all Operation performed on the IOCs").Register(*registry)),
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