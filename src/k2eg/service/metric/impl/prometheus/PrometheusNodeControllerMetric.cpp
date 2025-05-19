#include <k2eg/service/metric/INodeControllerMetric.h>

#include <k2eg/service/metric/impl/prometheus/PrometheusNodeControllerMetric.h>

using namespace prometheus;

using namespace k2eg::service::metric;
using namespace k2eg::service::metric::impl::prometheus_impl;

PrometheusNodeControllerMetric::PrometheusNodeControllerMetric()
    : registry(std::make_shared<Registry>())
    , node_controller_counter(
          BuildCounter().Name("k2eg_node_controller").Help("Is the set of metric for the ndoe controller layer").Register(*registry))
    , idle_counter_family(
          BuildCounter().Name("k2eg_node_controller_thread_idle_cycles_total").Help("Total number of idle cycles per thread").Register(*registry))
    , event_counter_family(
          BuildCounter().Name("k2eg_node_controller_thread_events_processed_total").Help("Total number of events processed per thread").Register(*registry))
    , duration_counter_family(prometheus::BuildCounter()
                                  .Name("k2eg_node_controller_thread_poll_duration_microseconds")
                                  .Help("Total poll time in microseconds per thread")
                                  .Register(*registry))
    , throttle_gauge_family(BuildGauge().Name("k2eg_node_controller_thread_throttle_ms").Help("Current throttle delay per thread in ms").Register(*registry))
    , submitted_command(node_controller_counter.Add({{"op", "command_submitted"}}))
{
}

void PrometheusNodeControllerMetric::incrementCounter(INodeControllerMetricCounterType type, const double inc_value, const std::map<std::string, std::string>& label)
{
    switch (type)
    {
    case INodeControllerMetricCounterType::SubmittedCommand: submitted_command.Increment(inc_value); break;
    case INodeControllerMetricCounterType::ThrottlingIdleCounter: idle_counter_family.Add(label).Increment(inc_value); break;
    case INodeControllerMetricCounterType::ThrottlingEventCounter: event_counter_family.Add(label).Increment(inc_value); break;
    case INodeControllerMetricCounterType::ThrottlingDurationCounter:
        duration_counter_family.Add(label).Increment(inc_value);
        break;
    case INodeControllerMetricCounterType::ThrottleGauge: throttle_gauge_family.Add(label).Set(inc_value); break;
    }
}