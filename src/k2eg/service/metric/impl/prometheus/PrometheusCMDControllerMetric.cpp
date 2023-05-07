#include <k2eg/service/metric/impl/prometheus/PrometheusCMDControllerMetric.h>

using namespace prometheus;

using namespace k2eg::service::metric;
using namespace k2eg::service::metric::impl::prometheus_impl;

PrometheusCMDControllerMetric::PrometheusCMDControllerMetric()
    : registry(std::make_shared<Registry>()),
      command_received_counters(
          BuildCounter().Name("k2eg_command_controller").Help("Is the metric set for the reception and decode of the command").Register(*registry)),
      received(command_received_counters.Add({{"op", "received"}})),
      bad_parsed(command_received_counters.Add({{"op", "bad_command"}})),
      fault_processed(command_received_counters.Add({{"op", "fault_processed"}})) {}

void
PrometheusCMDControllerMetric::incrementCounter(ICMDControllerMetricCounterType type, double inc_value) {
  switch (type) {
    case ICMDControllerMetricCounterType::ReceivedCommand: received.Increment(inc_value); break;
    case ICMDControllerMetricCounterType::BadCommand: bad_parsed.Increment(inc_value); break;
    case ICMDControllerMetricCounterType::FaultProcessingCommand: fault_processed.Increment(inc_value); break;
  }
}