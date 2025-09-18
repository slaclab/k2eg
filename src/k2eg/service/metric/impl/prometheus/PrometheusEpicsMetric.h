#ifndef K2EG_SERVICE_METRIC_IMPL_PROMETHEUS_PROMETHEUSEPICSMETRIC_H_
#define K2EG_SERVICE_METRIC_IMPL_PROMETHEUS_PROMETHEUSEPICSMETRIC_H_

#include <k2eg/service/metric/IEpicsMetric.h>

#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/gauge.h>
#include <prometheus/labels.h>
#include <prometheus/registry.h>

#include <atomic>
#include <memory>
#include <thread>

namespace k2eg::service::metric::impl::prometheus_impl {

class PrometheusMetricService;

// low level api provider for metric based on prometheus
class PrometheusEpicsMetric : public IEpicsMetric
{
    friend class PrometheusMetricService;
    std::shared_ptr<prometheus::Registry>    registry;
    prometheus::Family<prometheus::Counter>& ioc_read_write;
    prometheus::Family<prometheus::Gauge>&   ioc_read_write_rate;
    prometheus::Family<prometheus::Gauge>&   ioc_pv_gauge;
    prometheus::Family<prometheus::Gauge>&   duration_gauge_family;
    prometheus::Family<prometheus::Counter>& event_counter_family;
    prometheus::Family<prometheus::Gauge>&   idle_gauge_family;
    prometheus::Family<prometheus::Gauge>&   throttle_gauge_family;
    prometheus::Family<prometheus::Gauge>&   pv_throttle_gauge_family;
    prometheus::Family<prometheus::Counter>& pv_backlog_counter_family;
    prometheus::Family<prometheus::Gauge>&   pv_processing_duration_gauge_family;
    prometheus::Counter&                     get_ok_counter;
    prometheus::Counter&                     put_ok_counter;
    prometheus::Counter&                     monitor_event_data;
    prometheus::Gauge&                       monitor_event_data_rate_sec;
    prometheus::Counter&                     monitor_event_fail;
    prometheus::Counter&                     monitor_event_cancel;
    prometheus::Counter&                     monitor_event_disconnected;
    prometheus::Gauge&                       total_monitor_pv_gauge;
    prometheus::Gauge&                       active_monitor_pv_gauge;

    PrometheusEpicsMetric();

    bool                                  run_rate_thread;
    std::chrono::steady_clock::time_point start_sample_ts;
    std::thread                           rate_thread;
    std::atomic<double>                   monitor_event_data_rate_sec_count;
    inline void                           calcRateThread();

public:
    virtual ~PrometheusEpicsMetric();
    void incrementCounter(IEpicsMetricCounterType type, const double inc_value = 1.0, const std::map<std::string, std::string>& label = {}) override final;
};
} // namespace k2eg::service::metric::impl::prometheus_impl

#endif // K2EG_SERVICE_METRIC_IMPL_PROMETHEUS_PROMETHEUSEPICSMETRIC_H_
