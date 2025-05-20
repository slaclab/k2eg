#include <k2eg/service/metric/impl/prometheus/PrometheusEpicsMetric.h>

#include <prometheus/gauge.h>

#include <chrono>
#include <cstdint>
#include <thread>


using namespace prometheus;

using namespace k2eg::service::metric;
using namespace k2eg::service::metric::impl::prometheus_impl;

PrometheusEpicsMetric::PrometheusEpicsMetric()
    : registry(std::make_shared<Registry>())
    , ioc_read_write(
          BuildCounter().Name("k2eg_epics_ioc_operation").Help("Metric set for all Operation performed on the IOCs").Register(*registry))
    , ioc_read_write_rate(BuildGauge()
                              .Name("k2eg_epics_ioc_operation_rate")
                              .Help("Metric set for all Operation Rate performed on the IOCs")
                              .Register(*registry))
    , ioc_pv_count(BuildGauge().Name("k2eg_epics_ioc_pv_count").Help("Metric set for all pv counting information").Register(*registry))
    , idle_counter_family(
          BuildCounter().Name("k2eg_epics_thread_idle_cycles_total").Help("Total number of idle cycles per thread").Register(*registry))
    , event_counter_family(
          BuildCounter().Name("k2eg_epics_thread_events_processed_total").Help("Total number of events processed per thread").Register(*registry))
    , duration_counter_family(prometheus::BuildCounter()
                                  .Name("k2eg_epics_thread_poll_duration_microseconds")
                                  .Help("Total poll time in microseconds per thread")
                                  .Register(*registry))
    , throttle_gauge_family(BuildGauge().Name("k2eg_epics_thread_throttle_ms").Help("Current throttle delay per thread in ms").Register(*registry))
    , get_ok_counter(ioc_read_write.Add({{"op", "get"}}))
    , put_ok_counter(ioc_read_write.Add({{"op", "put"}}))
    , monitor_event_data(ioc_read_write.Add({{"op", "monitor"}, {"evt_type", "data"}}))
    , monitor_event_data_rate_sec(ioc_read_write_rate.Add({{"op", "monitor"}, {"evt_type", "data"}}))
    , monitor_event_fail(ioc_read_write.Add({{"op", "monitor"}, {"evt_type", "fail"}}))
    , monitor_event_cancel(ioc_read_write.Add({{"op", "monitor"}, {"evt_type", "cancel"}}))
    , monitor_event_disconnected(ioc_read_write.Add({{"op", "monitor"}, {"evt_type", "disconnect"}}))
    , total_monitor_pv(ioc_pv_count.Add({{"type", "total"}}))
    , active_monitor_pv(ioc_pv_count.Add({{"type", "active"}}))
    , run_rate_thread(true)
    , start_sample_ts(std::chrono::steady_clock::now())
    , rate_thread(&PrometheusEpicsMetric::calcRateThread, this)
    , monitor_event_data_rate_sec_count(0)
{
}

PrometheusEpicsMetric::~PrometheusEpicsMetric()
{
    run_rate_thread = false;
    rate_thread.join();
}

void PrometheusEpicsMetric::calcRateThread()
{
    while (run_rate_thread)
    {
        double rate_per_sec = 0;
        auto   end_time = std::chrono::steady_clock::now();
        auto   elapsed = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_sample_ts);
        if (elapsed.count() >= 5)
        {
            rate_per_sec = monitor_event_data_rate_sec_count.load() / elapsed.count();
            monitor_event_data_rate_sec_count = 0;
            start_sample_ts = end_time;
            monitor_event_data_rate_sec.Set(rate_per_sec);
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void PrometheusEpicsMetric::incrementCounter(IEpicsMetricCounterType type, const double inc_value, const std::map<std::string, std::string>& label)
{
    switch (type)
    {
    case IEpicsMetricCounterType::Get: get_ok_counter.Increment(inc_value); break;
    case IEpicsMetricCounterType::Put: put_ok_counter.Increment(inc_value); break;
    case IEpicsMetricCounterType::MonitorData:
        monitor_event_data.Increment(inc_value);
        monitor_event_data_rate_sec_count += (int64_t)inc_value;
        break;
    case IEpicsMetricCounterType::MonitorFail: monitor_event_fail.Increment(inc_value); break;
    case IEpicsMetricCounterType::MonitorCancel: monitor_event_cancel.Increment(inc_value); break;
    case IEpicsMetricCounterType::MonitorDisconnect: monitor_event_disconnected.Increment(inc_value); break;
    case IEpicsMetricCounterType::MonitorTimeout: break;
    case IEpicsMetricCounterType::TotalMonitor: total_monitor_pv.Set(inc_value); break;
    case IEpicsMetricCounterType::ActiveMonitor: active_monitor_pv.Set(inc_value); break;
    case IEpicsMetricCounterType::ThrottlingIdleCounter: idle_counter_family.Add(label).Increment(inc_value); break;
    case IEpicsMetricCounterType::ThrottlingEventCounter: event_counter_family.Add(label).Increment(inc_value); break;
    case IEpicsMetricCounterType::ThrottlingDurationCounter:
        duration_counter_family.Add(label).Increment(inc_value);
        break;
    case IEpicsMetricCounterType::ThrottleGauge: throttle_gauge_family.Add(label).Set(inc_value); break;
    }
}