
#include <k2eg/service/metric/impl/prometheus/PrometheusNodeControllerSystemMetric.h>

#include <thread>

using namespace prometheus;
using namespace k2eg::service::metric;
using namespace k2eg::service::metric::impl::prometheus_impl;

struct CounterState
{
    double last_value = 0.0;
    bool   initialized = false;
};

// Map: metric type + label hash -> state
std::mutex                                    counter_state_mutex;
std::unordered_map<std::string, CounterState> counter_states;

std::string make_counter_key(INodeControllerSystemMetricType type, const std::map<std::string, std::string>& label)
{
    std::string key = std::to_string(static_cast<int>(type));
    for (const auto& kv : label)
    {
        key += "|" + kv.first + "=" + kv.second;
    }
    return key;
}

PrometheusNodeControllerSystemMetric::PrometheusNodeControllerSystemMetric()
    : registry(std::make_shared<prometheus::Registry>())
    , utime_ticks_counter_family(prometheus::BuildCounter()
                                     .Name("k2eg_system_utime_ticks_total")
                                     .Help("Total user mode ticks used by the process")
                                     .Register(*registry))
    , stime_ticks_counter_family(prometheus::BuildCounter()
                                     .Name("k2eg_system_stime_ticks_total")
                                     .Help("Total kernel mode ticks used by the process")
                                     .Register(*registry))
    , cpu_seconds_counter_family(prometheus::BuildCounter()
                                     .Name("k2eg_system_cpu_seconds_total")
                                     .Help("Total CPU seconds (user+sys) used by the process")
                                     .Register(*registry))
    , io_read_bytes_counter_family(
          prometheus::BuildCounter().Name("k2eg_system_io_read_bytes_total").Help("Total IO read bytes by the process").Register(*registry))
    , io_write_bytes_counter_family(prometheus::BuildCounter()
                                        .Name("k2eg_system_io_write_bytes_total")
                                        .Help("Total IO write bytes by the process")
                                        .Register(*registry))
    , voluntary_ctxt_switches_counter_family(prometheus::BuildCounter()
                                                 .Name("k2eg_system_voluntary_ctxt_switches_total")
                                                 .Help("Total voluntary context switches by the process")
                                                 .Register(*registry))
    , nonvoluntary_ctxt_switches_counter_family(prometheus::BuildCounter()
                                                    .Name("k2eg_system_nonvoluntary_ctxt_switches_total")
                                                    .Help("Total nonvoluntary context switches by the process")
                                                    .Register(*registry))
    , vmrss_gauge_family(prometheus::BuildGauge().Name("k2eg_system_vmrss_kb").Help("Resident Set Size (kB)").Register(*registry))
    , vmhwm_gauge_family(prometheus::BuildGauge().Name("k2eg_system_vmhwm_kb").Help("Peak Resident Set Size (kB)").Register(*registry))
    , vmsize_gauge_family(prometheus::BuildGauge().Name("k2eg_system_vmsize_kb").Help("Virtual Memory Size (kB)").Register(*registry))
    , vmdata_gauge_family(prometheus::BuildGauge().Name("k2eg_system_vmdata_kb").Help("Data Segment Size (kB)").Register(*registry))
    , vmswap_gauge_family(prometheus::BuildGauge().Name("k2eg_system_vmswap_kb").Help("Swap Size (kB)").Register(*registry))
    , rss_anon_gauge_family(prometheus::BuildGauge().Name("k2eg_system_rss_anon_kb").Help("Anonymous Memory Size (kB)").Register(*registry))
    , rss_file_gauge_family(prometheus::BuildGauge().Name("k2eg_system_rss_file_kb").Help("File-backed Memory Size (kB)").Register(*registry))
    , threads_gauge_family(prometheus::BuildGauge().Name("k2eg_system_threads").Help("Number of threads").Register(*registry))
    , open_fds_gauge_family(
          prometheus::BuildGauge().Name("k2eg_system_open_fds").Help("Number of open file descriptors").Register(*registry))
    , max_fds_gauge_family(
          prometheus::BuildGauge().Name("k2eg_system_max_fds").Help("Maximum number of file descriptors").Register(*registry))
    , uptime_sec_gauge_family(prometheus::BuildGauge().Name("k2eg_system_uptime_seconds").Help("Uptime in seconds").Register(*registry))
    , starttime_jiffies_gauge_family(
          prometheus::BuildGauge().Name("k2eg_system_starttime_jiffies").Help("Process start time (jiffies since boot)").Register(*registry))
    , num_cores_gauge_family(prometheus::BuildGauge().Name("k2eg_system_num_cores").Help("Number of logical CPU cores").Register(*registry))
{
    // Additional initialization if needed
    unsigned int num_cores = std::thread::hardware_concurrency();
    num_cores_gauge_family.Add({}).Set(static_cast<double>(num_cores));
}

void PrometheusNodeControllerSystemMetric::incrementCounter(INodeControllerSystemMetricType type, const double absolute_value, const std::map<std::string, std::string>& label)
{
    // Only for counters: compute delta and increment
    auto is_counter = [](INodeControllerSystemMetricType t)
    {
        switch (t)
        {
        case INodeControllerSystemMetricType::UtimeTicksCounter:
        case INodeControllerSystemMetricType::StimeTicksCounter:
        case INodeControllerSystemMetricType::CpuSecondsCounter:
        case INodeControllerSystemMetricType::IOReadBytesCounter:
        case INodeControllerSystemMetricType::IOWriteBytesCounter:
        case INodeControllerSystemMetricType::VoluntaryCtxtSwitchesCounter:
        case INodeControllerSystemMetricType::NonvoluntaryCtxtSwitchesCounter: return true;
        default: return false;
        }
    };

    if (is_counter(type))
    {
        std::string key = make_counter_key(type, label);
        double      delta = 0.0;
        {
            std::lock_guard<std::mutex> lock(counter_state_mutex);
            auto&                       state = counter_states[key];
            if (state.initialized)
            {
                delta = absolute_value - state.last_value;
                // Only increment if delta is positive (handle process restart or counter reset)
                if (delta > 0)
                {
                    state.last_value = absolute_value;
                }
                else
                {
                    // Counter reset or no progress, just update last_value
                    state.last_value = absolute_value;
                    delta = 0.0;
                }
            }
            else
            {
                // First observation, add the absolute value to the counter
                delta = absolute_value;
                state.last_value = absolute_value;
                state.initialized = true;
            }
        }
        if (delta > 0)
        {
            switch (type)
            {
            case INodeControllerSystemMetricType::UtimeTicksCounter:
                utime_ticks_counter_family.Add(label).Increment(delta);
                break;
            case INodeControllerSystemMetricType::StimeTicksCounter:
                stime_ticks_counter_family.Add(label).Increment(delta);
                break;
            case INodeControllerSystemMetricType::CpuSecondsCounter:
                cpu_seconds_counter_family.Add(label).Increment(delta);
                break;
            case INodeControllerSystemMetricType::IOReadBytesCounter:
                io_read_bytes_counter_family.Add(label).Increment(delta);
                break;
            case INodeControllerSystemMetricType::IOWriteBytesCounter:
                io_write_bytes_counter_family.Add(label).Increment(delta);
                break;
            case INodeControllerSystemMetricType::VoluntaryCtxtSwitchesCounter:
                voluntary_ctxt_switches_counter_family.Add(label).Increment(delta);
                break;
            case INodeControllerSystemMetricType::NonvoluntaryCtxtSwitchesCounter:
                nonvoluntary_ctxt_switches_counter_family.Add(label).Increment(delta);
                break;
            default: break;
            }
        }
    }
    else
    {
        // Gauges: set the value directly
        switch (type)
        {
        case INodeControllerSystemMetricType::VmRSSGauge: vmrss_gauge_family.Add(label).Set(absolute_value); break;
        case INodeControllerSystemMetricType::VmHWMGauge: vmhwm_gauge_family.Add(label).Set(absolute_value); break;
        case INodeControllerSystemMetricType::VmSizeGauge: vmsize_gauge_family.Add(label).Set(absolute_value); break;
        case INodeControllerSystemMetricType::VmDataGauge: vmdata_gauge_family.Add(label).Set(absolute_value); break;
        case INodeControllerSystemMetricType::VmSwapGauge: vmswap_gauge_family.Add(label).Set(absolute_value); break;
        case INodeControllerSystemMetricType::RssAnonGauge: rss_anon_gauge_family.Add(label).Set(absolute_value); break;
        case INodeControllerSystemMetricType::RssFileGauge: rss_file_gauge_family.Add(label).Set(absolute_value); break;
        case INodeControllerSystemMetricType::ThreadsGauge: threads_gauge_family.Add(label).Set(absolute_value); break;
        case INodeControllerSystemMetricType::OpenFDsGauge: open_fds_gauge_family.Add(label).Set(absolute_value); break;
        case INodeControllerSystemMetricType::MaxFDsGauge: max_fds_gauge_family.Add(label).Set(absolute_value); break;
        case INodeControllerSystemMetricType::UptimeSecGauge:
            uptime_sec_gauge_family.Add(label).Set(absolute_value);
            break;
        case INodeControllerSystemMetricType::StarttimeJiffiesGauge:
            starttime_jiffies_gauge_family.Add(label).Set(absolute_value);
            break;
        default: break;
        }
    }
}