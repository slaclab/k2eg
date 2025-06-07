#ifndef K2EG_COMMON_PROCSYSTEMMETRICS_H_
#define K2EG_COMMON_PROCSYSTEMMETRICS_H_

namespace k2eg::common {

/**
 * @brief Collects and stores various system and process metrics.
 *
 * This class provides methods to refresh and access information about
 * memory usage, CPU usage, thread count, file descriptors, IO statistics,
 * context switches, process uptime, and process start time.
 */
class ProcSystemMetrics
{
public:
    unsigned int num_cores;
    // Memory (in kB)
    long vm_rss = 0;   ///< Resident Set Size: non-swapped physical memory
    long vm_hwm = 0;   ///< Peak Resident Set Size ("high water mark")
    long vm_size = 0;  ///< Virtual memory size
    long vm_data = 0;  ///< Size of data segment
    long vm_swap = 0;  ///< Swapped-out virtual memory size

    // CPU
    long   utime_ticks = 0; ///< User mode CPU time (clock ticks)
    long   stime_ticks = 0; ///< Kernel mode CPU time (clock ticks)
    double cpu_seconds = 0; ///< Total CPU time (user + sys) in seconds

    // Threads
    long threads = 0; ///< Number of threads in the process

    // File descriptors
    long open_fds = 0; ///< Number of open file descriptors
    long max_fds = 0;  ///< Maximum allowed file descriptors

    // Uptime
    double uptime_sec = 0; ///< Process uptime in seconds

    // IO
    long long io_read_bytes = 0;  ///< Bytes read by the process
    long long io_write_bytes = 0; ///< Bytes written by the process

    // Context switches
    long voluntary_ctxt_switches = 0;    ///< Voluntary context switches
    long nonvoluntary_ctxt_switches = 0; ///< Non-voluntary context switches

    // Process start time (jiffies since boot)
    long long starttime_jiffies = 0; ///< Process start time in jiffies

    /**
     * @brief Constructs a ProcSystemMetrics object with all metrics initialized to zero.
     */
    ProcSystemMetrics();

    /**
     * @brief Refreshes all metrics by reading from the system.
     */
    void refresh();

private:
    /// Reads memory and thread statistics from /proc/[pid]/status
    void readStatus();
    /// Reads CPU and start time statistics from /proc/[pid]/stat
    void readStat();
    /// Reads file descriptor limits from /proc/[pid]/limits
    void readLimits();
    /// Counts open file descriptors in /proc/[pid]/fd
    void readOpenFDs();
    /// Reads IO statistics from /proc/[pid]/io
    void readIO();
    /// Calculates process uptime based on system uptime and process start time
    void calculateUptime();
};
} // namespace k2eg::common

#endif // K2EG_COMMON_PROCSYSTEMMETRICS_H_