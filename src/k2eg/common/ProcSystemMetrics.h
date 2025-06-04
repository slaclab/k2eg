#ifndef K2EG_COMMON_PROCSYSTEMMETRICS_H_
#define K2EG_COMMON_PROCSYSTEMMETRICS_H_

namespace k2eg::common {

class ProcSystemMetrics
{
public:
    // Memory
    long vm_rss = 0;   // kB
    long vm_hwm = 0;   // kB
    long vm_size = 0;  // kB
    long vm_data = 0;  // kB
    long vm_swap = 0;  // kB

    // CPU
    long   utime_ticks = 0; // user mode ticks
    long   stime_ticks = 0; // kernel mode ticks
    double cpu_seconds = 0; // (user+sys) seconds

    // Threads
    long threads = 0;

    // File descriptors
    long open_fds = 0;
    long max_fds = 0;

    // Uptime
    double uptime_sec = 0;

    // IO
    long long io_read_bytes = 0;
    long long io_write_bytes = 0;

    // Context switches
    long voluntary_ctxt_switches = 0;
    long nonvoluntary_ctxt_switches = 0;

    // Process start time (jiffies since boot)
    long long starttime_jiffies = 0;

    ProcSystemMetrics();
    void refresh();

private:
    void readStatus();
    void readStat();
    void readLimits();
    void readOpenFDs();
    void readIO();
    void calculateUptime();
};
} // namespace k2eg::common

#endif // K2EG_COMMON_PROCSYSTEMMETRICS_H_