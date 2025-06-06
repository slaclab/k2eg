
#include <k2eg/service/metric/impl/prometheus/PrometheusNodeControllerSystemMetric.h>

using namespace prometheus;
using namespace k2eg::service::metric;
using namespace k2eg::service::metric::impl::prometheus_impl;

PrometheusNodeControllerSystemMetric::PrometheusNodeControllerSystemMetric()
    : registry(std::make_shared<prometheus::Registry>()),
      utime_ticks_counter_family(prometheus::BuildCounter()
          .Name("k2eg_system_utime_ticks_total")
          .Help("Total user mode ticks used by the process")
          .Register(*registry)),
      stime_ticks_counter_family(prometheus::BuildCounter()
          .Name("k2eg_system_stime_ticks_total")
          .Help("Total kernel mode ticks used by the process")
          .Register(*registry)),
      cpu_seconds_counter_family(prometheus::BuildCounter()
          .Name("k2eg_system_cpu_seconds_total")
          .Help("Total CPU seconds (user+sys) used by the process")
          .Register(*registry)),
      io_read_bytes_counter_family(prometheus::BuildCounter()
          .Name("k2eg_system_io_read_bytes_total")
          .Help("Total IO read bytes by the process")
          .Register(*registry)),
      io_write_bytes_counter_family(prometheus::BuildCounter()
          .Name("k2eg_system_io_write_bytes_total")
          .Help("Total IO write bytes by the process")
          .Register(*registry)),
      voluntary_ctxt_switches_counter_family(prometheus::BuildCounter()
          .Name("k2eg_system_voluntary_ctxt_switches_total")
          .Help("Total voluntary context switches by the process")
          .Register(*registry)),
      nonvoluntary_ctxt_switches_counter_family(prometheus::BuildCounter()
          .Name("k2eg_system_nonvoluntary_ctxt_switches_total")
          .Help("Total nonvoluntary context switches by the process")
          .Register(*registry)),
      vmrss_gauge_family(prometheus::BuildGauge()
          .Name("k2eg_system_vmrss_kb")
          .Help("Resident Set Size (kB)")
          .Register(*registry)),
      vmhwm_gauge_family(prometheus::BuildGauge()
          .Name("k2eg_system_vmhwm_kb")
          .Help("Peak Resident Set Size (kB)")
          .Register(*registry)),
      vmsize_gauge_family(prometheus::BuildGauge()
          .Name("k2eg_system_vmsize_kb")
          .Help("Virtual Memory Size (kB)")
          .Register(*registry)),
      vmdata_gauge_family(prometheus::BuildGauge()
          .Name("k2eg_system_vmdata_kb")
          .Help("Data Segment Size (kB)")
          .Register(*registry)),
      vmswap_gauge_family(prometheus::BuildGauge()
          .Name("k2eg_system_vmswap_kb")
          .Help("Swap Size (kB)")
          .Register(*registry)),
      threads_gauge_family(prometheus::BuildGauge()
          .Name("k2eg_system_threads")
          .Help("Number of threads")
          .Register(*registry)),
      open_fds_gauge_family(prometheus::BuildGauge()
          .Name("k2eg_system_open_fds")
          .Help("Number of open file descriptors")
          .Register(*registry)),
      max_fds_gauge_family(prometheus::BuildGauge()
          .Name("k2eg_system_max_fds")
          .Help("Maximum number of file descriptors")
          .Register(*registry)),
      uptime_sec_gauge_family(prometheus::BuildGauge()
          .Name("k2eg_system_uptime_seconds")
          .Help("Uptime in seconds")
          .Register(*registry)),
      starttime_jiffies_gauge_family(prometheus::BuildGauge()
          .Name("k2eg_system_starttime_jiffies")
          .Help("Process start time (jiffies since boot)")
          .Register(*registry))
{
    // Additional initialization if needed
}

void PrometheusNodeControllerSystemMetric::incrementCounter(INodeControllerSystemMetricType type, const double inc_value, const std::map<std::string, std::string>& label){
    switch (type) {
        case INodeControllerSystemMetricType::UtimeTicksCounter:
            utime_ticks_counter_family.Add(label).Increment(inc_value);
            break;
        case INodeControllerSystemMetricType::StimeTicksCounter:
            stime_ticks_counter_family.Add(label).Increment(inc_value);
            break;
        case INodeControllerSystemMetricType::CpuSecondsCounter:
            cpu_seconds_counter_family.Add(label).Increment(inc_value);
            break;
        case INodeControllerSystemMetricType::IOReadBytesCounter:
            io_read_bytes_counter_family.Add(label).Increment(inc_value);
            break;
        case INodeControllerSystemMetricType::IOWriteBytesCounter:
            io_write_bytes_counter_family.Add(label).Increment(inc_value);
            break;
        case INodeControllerSystemMetricType::VoluntaryCtxtSwitchesCounter:
            voluntary_ctxt_switches_counter_family.Add(label).Increment(inc_value);
            break;
        case INodeControllerSystemMetricType::NonvoluntaryCtxtSwitchesCounter:
            nonvoluntary_ctxt_switches_counter_family.Add(label).Increment(inc_value);
            break;
        case INodeControllerSystemMetricType::VmRSSGauge:
            vmrss_gauge_family.Add(label).Set(inc_value);
            break;
        case INodeControllerSystemMetricType::VmHWMGauge:
            vmhwm_gauge_family.Add(label).Set(inc_value);
            break;
        case INodeControllerSystemMetricType::VmSizeGauge:
            vmsize_gauge_family.Add(label).Set(inc_value);
            break;
        case INodeControllerSystemMetricType::VmDataGauge:
            vmdata_gauge_family.Add(label).Set(inc_value);
            break;
        case INodeControllerSystemMetricType::VmSwapGauge:
            vmswap_gauge_family.Add(label).Set(inc_value);
            break;
        case INodeControllerSystemMetricType::ThreadsGauge:
            threads_gauge_family.Add(label).Set(inc_value);
            break;
        case INodeControllerSystemMetricType::OpenFDsGauge:
            open_fds_gauge_family.Add(label).Set(inc_value);
            break;
        case INodeControllerSystemMetricType::MaxFDsGauge:
            max_fds_gauge_family.Add(label).Set(inc_value);
            break;
        case INodeControllerSystemMetricType::UptimeSecGauge:
            uptime_sec_gauge_family.Add(label).Set(inc_value);
            break;
        case INodeControllerSystemMetricType::StarttimeJiffiesGauge:
            starttime_jiffies_gauge_family.Add(label).Set(inc_value);
            break;
    }
}
