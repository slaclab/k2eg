#include <k2eg/service/metric/INodeControllerMetric.h>

#include <k2eg/service/metric/impl/prometheus/PrometheusNodeControllerMetric.h>

using namespace prometheus;

using namespace k2eg::service::metric;
using namespace k2eg::service::metric::impl::prometheus_impl;

PrometheusNodeControllerMetric::PrometheusNodeControllerMetric()
    : registry(std::make_shared<Registry>())
    , node_controller_command_family(
          BuildCounter().Name("k2eg_node_controller_submitted_command_counter").Help("The numer of submitted command into the node controller").Register(*registry))
    , snapshot_event_counter_family(
          BuildCounter().Name("k2eg_node_controller_snapshot_event_counter").Help("Total number of publishing events processed per thread").Register(*registry))
{
}

void PrometheusNodeControllerMetric::incrementCounter(INodeControllerMetricCounterType type, const double inc_value, const std::map<std::string, std::string>& label)
{
    switch (type)
    {
    case INodeControllerMetricCounterType::SubmittedCommand: node_controller_command_family.Add(label).Increment(inc_value); break;
    case INodeControllerMetricCounterType::SnapshotEventCounter: snapshot_event_counter_family.Add(label).Increment(inc_value); break;
    }
}