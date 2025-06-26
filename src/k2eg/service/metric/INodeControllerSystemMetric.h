#ifndef K2EG_SERVICE_METRIC_ISYSTEMSTAT_H_
#define K2EG_SERVICE_METRIC_ISYSTEMSTAT_H_

#include "k2eg/service/metric/INodeControllerMetric.h"

namespace k2eg::service::metric {

// epics counter types
enum class INodeControllerSystemMetricType
{
    // Memory metrics (Gauge)
    VmRSSGauge,  // Resident Set Size (kB)
    VmHWMGauge,  // Peak Resident Set Size (kB)
    VmSizeGauge, // Virtual Memory Size (kB)
    VmDataGauge, // Data Segment Size (kB)
    VmSwapGauge, // Swap Size (kB)
    RssAnonGauge, // Anonymous Memory Size (kB)
    RssFileGauge, // File-backed Memory Size (kB)
    
    // CPU metrics (Counter)
    UtimeTicksCounter, // User mode ticks
    StimeTicksCounter, // Kernel mode ticks
    CpuSecondsCounter, // Total CPU seconds (user+sys)

    // Threads (Gauge)
    ThreadsGauge, // Number of threads

    // File descriptors (Gauge)
    OpenFDsGauge, // Number of open file descriptors
    MaxFDsGauge,  // Maximum number of file descriptors

    // Uptime (Gauge)
    UptimeSecGauge, // Uptime in seconds

    // IO (Counter)
    IOReadBytesCounter,  // IO read bytes
    IOWriteBytesCounter, // IO write bytes

    // Context switches (Counter)
    VoluntaryCtxtSwitchesCounter,    // Voluntary context switches
    NonvoluntaryCtxtSwitchesCounter, // Nonvoluntary context switches

    // Process start time (Gauge)
    StarttimeJiffiesGauge // Process start time (jiffies since boot)
};

// Structure that collect all the systme statistics
class INodeControllerSystemMetric
{
    friend class IMetricService;

public:
    INodeControllerSystemMetric() = default;
    virtual ~INodeControllerSystemMetric() = default;
    virtual void incrementCounter(INodeControllerSystemMetricType type, const double inc_value = 1.0, const std::map<std::string, std::string>& label = {}) = 0;
};

} // namespace k2eg::service::metric

#endif // K2EG_SERVICE_METRIC_ISYSTEMSTAT_H_