#ifndef K2EG_SERVICE_METRIC_IMPL_PROMETHEUS_PROMETHEUSNODECONTROLLERSYSTEMMETRIC_H_
#define K2EG_SERVICE_METRIC_IMPL_PROMETHEUS_PROMETHEUSNODECONTROLLERSYSTEMMETRIC_H_

#include <k2eg/service/metric/INodeControllerSystemMetric.h>

#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/gauge.h>
#include <prometheus/labels.h>
#include <prometheus/registry.h>

namespace k2eg::service::metric::impl::prometheus_impl {

class PrometheusMetricService;

// low level api provider for metric based on prometheus
class PrometheusNodeControllerSystemMetric : public INodeControllerSystemMetric
{
    friend class PrometheusMetricService;

    std::shared_ptr<prometheus::Registry> registry;
    // Prometheus metric families for system metrics
    prometheus::Family<prometheus::Counter>& utime_ticks_counter_family;
    prometheus::Family<prometheus::Counter>& stime_ticks_counter_family;
    prometheus::Family<prometheus::Counter>& cpu_seconds_counter_family;
    prometheus::Family<prometheus::Counter>& io_read_bytes_counter_family;
    prometheus::Family<prometheus::Counter>& io_write_bytes_counter_family;
    prometheus::Family<prometheus::Counter>& voluntary_ctxt_switches_counter_family;
    prometheus::Family<prometheus::Counter>& nonvoluntary_ctxt_switches_counter_family;

    prometheus::Family<prometheus::Gauge>& vmrss_gauge_family;
    prometheus::Family<prometheus::Gauge>& vmhwm_gauge_family;
    prometheus::Family<prometheus::Gauge>& vmsize_gauge_family;
    prometheus::Family<prometheus::Gauge>& vmdata_gauge_family;
    prometheus::Family<prometheus::Gauge>& vmswap_gauge_family;
    prometheus::Family<prometheus::Gauge>& threads_gauge_family;
    prometheus::Family<prometheus::Gauge>& open_fds_gauge_family;
    prometheus::Family<prometheus::Gauge>& max_fds_gauge_family;
    prometheus::Family<prometheus::Gauge>& uptime_sec_gauge_family;
    prometheus::Family<prometheus::Gauge>& starttime_jiffies_gauge_family;
    prometheus::Family<prometheus::Gauge>& num_cores_gauge_family;

    PrometheusNodeControllerSystemMetric();

public:
    virtual ~PrometheusNodeControllerSystemMetric() = default;
    void incrementCounter(INodeControllerSystemMetricType type, const double inc_value = 1.0, const std::map<std::string, std::string>& label = {}) override final;
};

} // namespace k2eg::service::metric::impl::prometheus_impl

#endif // K2EG_SERVICE_METRIC_IMPL_PROMETHEUS_PROMETHEUSNODECONTROLLERSYSTEMMETRIC_H_