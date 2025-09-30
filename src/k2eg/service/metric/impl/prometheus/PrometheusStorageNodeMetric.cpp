#include <k2eg/service/metric/impl/prometheus/PrometheusStorageNodeMetric.h>

using namespace prometheus;
using namespace k2eg::service::metric;
using namespace k2eg::service::metric::impl::prometheus_impl;

PrometheusStorageNodeMetric::PrometheusStorageNodeMetric()
    : registry(std::make_shared<Registry>())
    , running_archivers_gauge_family(
          BuildGauge().Name("k2eg_storage_running_archivers").Help("Current number of running archivers").Register(*registry)) {}

void PrometheusStorageNodeMetric::incrementCounter(IStorageNodeMetricGaugeType type,
                                                   const double inc_value,
                                                   const std::map<std::string, std::string>& label) {
  switch (type) {
    case IStorageNodeMetricGaugeType::RunningArchivers:
      running_archivers_gauge_family.Add(label).Set(inc_value);
      break;
  }
}
