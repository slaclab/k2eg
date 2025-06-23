#ifndef K2EG_SERVICE_METRIC_IMPL_PROMETHEUS_PROMETHEUSNODECONTROLLERMETRIC_H_
#define K2EG_SERVICE_METRIC_IMPL_PROMETHEUS_PROMETHEUSNODECONTROLLERMETRIC_H_

#include <k2eg/service/metric/INodeControllerMetric.h>

#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/labels.h>
#include <prometheus/registry.h>

#include <memory>

namespace k2eg::service::metric::impl::prometheus_impl {

class PrometheusMetricService;

// low level api provider for metric based on prometheus
class PrometheusNodeControllerMetric : public INodeControllerMetric
{
    friend class PrometheusMetricService;
    std::shared_ptr<prometheus::Registry>    registry;
    prometheus::Family<prometheus::Counter>& node_controller_command_family;
    prometheus::Family<prometheus::Counter>& snapshot_event_counter_family;
    PrometheusNodeControllerMetric();

public:
    virtual ~PrometheusNodeControllerMetric() = default;
    void incrementCounter(INodeControllerMetricCounterType type, const double inc_value = 1.0, const std::map<std::string, std::string>& label = {}) override final;
};
} // namespace k2eg::service::metric::impl::prometheus_impl

#endif // K2EG_SERVICE_METRIC_IMPL_PROMETHEUS_PROMETHEUSNODECONTROLLERMETRIC_H_