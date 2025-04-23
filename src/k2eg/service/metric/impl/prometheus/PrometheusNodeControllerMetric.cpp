#include <k2eg/service/metric/impl/prometheus/PrometheusNodeControllerMetric.h>
#include "k2eg/service/metric/INodeControllerMetric.h"

using namespace prometheus;

using namespace k2eg::service::metric;
using namespace k2eg::service::metric::impl::prometheus_impl;


PrometheusNodeControllerMetric::PrometheusNodeControllerMetric()
    : registry(std::make_shared<Registry>()),
      node_controller_counter(BuildCounter().Name("k2eg_node_controller").Help("Is the set of metric for the ndoe controller layer").Register(*registry)),
      submitted_command(node_controller_counter.Add({{"op", "command_submitted"}})) {}

void PrometheusNodeControllerMetric::incrementCounter(INodeControllerMetricCounterType type, const double inc_value, const std::map<std::string, std::string>& label) {
  switch (type) {
    case INodeControllerMetricCounterType::SubmittedCommand: submitted_command.Increment(inc_value); break;
  }
}