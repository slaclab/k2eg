#include <k2eg/service/metric/impl/prometheus/PrometheusStorageNodeMetric.h>

using namespace prometheus;
using namespace k2eg::service::metric;
using namespace k2eg::service::metric::impl::prometheus_impl;

PrometheusStorageNodeMetric::PrometheusStorageNodeMetric()
    : registry(std::make_shared<Registry>())
    , running_archivers_gauge_family(
          BuildGauge().Name("k2eg_storage_running_archivers").Help("Current number of running archivers").Register(*registry))
    , pv_recorded_counter_family(
          BuildCounter().Name("k2eg_storage_recorded_pv_total").Help("Total number of records stored per PV").Register(*registry))
    , snapshot_recorded_counter_family(
          BuildCounter().Name("k2eg_storage_recorded_snapshot_total").Help("Total number of records stored per snapshot").Register(*registry))
{}

void PrometheusStorageNodeMetric::incrementCounter(IStorageNodeMetricType type,
                                                   const double inc_value,
                                                   const std::map<std::string, std::string>& label) {
  switch (type) {
    case IStorageNodeMetricType::RunningArchiversGauge:
      running_archivers_gauge_family.Add(label).Set(inc_value);
      break;
    case IStorageNodeMetricType::RecordedPVRecords:
      pv_recorded_counter_family.Add(label).Increment(inc_value);
      break;
    case IStorageNodeMetricType::RecordedSnapshotRecords:
      snapshot_recorded_counter_family.Add(label).Increment(inc_value);
      break;
  }
}
