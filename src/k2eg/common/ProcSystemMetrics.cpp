#include <k2eg/common/ProcSystemMetrics.h>

#include <dirent.h>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <unistd.h>

using namespace k2eg::common;

ProcSystemMetrics::ProcSystemMetrics() {}

void ProcSystemMetrics::refresh()
{
    readStatus();
    readStat();
    readLimits();
    readOpenFDs();
    readIO();
    calculateUptime();
}

void ProcSystemMetrics::readStatus()
{
    std::ifstream file("/proc/self/status");
    std::string   line;
    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        std::string        key;
        long               value;
        std::string        unit;
        if (line.find("VmRSS:") == 0)
        {
            iss >> key >> value >> unit;
            vm_rss = value;
        }
        else if (line.find("VmHWM:") == 0)
        {
            iss >> key >> value >> unit;
            vm_hwm = value;
        }
        else if (line.find("VmSize:") == 0)
        {
            iss >> key >> value >> unit;
            vm_size = value;
        }
        else if (line.find("VmData:") == 0)
        {
            iss >> key >> value >> unit;
            vm_data = value;
        }
        else if (line.find("VmSwap:") == 0)
        {
            iss >> key >> value >> unit;
            vm_swap = value;
        }
        else if (line.find("Threads:") == 0)
        {
            iss >> key >> value;
            threads = value;
        }
        else if (line.find("voluntary_ctxt_switches:") == 0)
        {
            iss >> key >> value;
            voluntary_ctxt_switches = value;
        }
        else if (line.find("nonvoluntary_ctxt_switches:") == 0)
        {
            iss >> key >> value;
            nonvoluntary_ctxt_switches = value;
        }
    }
}

void ProcSystemMetrics::readStat()
{
    std::ifstream file("/proc/self/stat");
    std::string   data;
    std::getline(file, data);
    std::istringstream iss(data);
    std::string        discard;
    long long          val = 0;
    int                field = 1;

    // parse according to /proc/[pid]/stat docs
    // 14: utime, 15: stime, 22: starttime
    for (; field <= 13; ++field)
        iss >> discard; // skip to 14
    iss >> utime_ticks;
    ++field; // 14: utime
    iss >> stime_ticks;
    ++field; // 15: stime
    for (; field < 22; ++field)
        iss >> discard; // skip to 22
    iss >> starttime_jiffies;
    cpu_seconds = (utime_ticks + stime_ticks) / (double)sysconf(_SC_CLK_TCK);
}

void ProcSystemMetrics::readLimits()
{
    std::ifstream file("/proc/self/limits");
    std::string   line;
    while (std::getline(file, line))
    {
        if (line.find("Max open files") == 0)
        {
            std::istringstream iss(line);
            std::string        t1, t2, t3;
            long               val;
            iss >> t1 >> t2 >> t3 >> val;
            max_fds = val;
            break;
        }
    }
}

void ProcSystemMetrics::readOpenFDs()
{
    long count = 0;
    DIR* dir = opendir("/proc/self/fd");
    if (dir)
    {
        while (readdir(dir))
            ++count;
        closedir(dir);
        count -= 2; // . and ..
    }
    open_fds = count;
}

void ProcSystemMetrics::readIO()
{
    std::ifstream file("/proc/self/io");
    std::string   line;
    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        std::string        key;
        long long          value;
        iss >> key >> value;
        if (key == "read_bytes:")
            io_read_bytes = value;
        else if (key == "write_bytes:")
            io_write_bytes = value;
    }
}

void ProcSystemMetrics::calculateUptime()
{
    // get process start time (from /proc/self/stat) and system uptime (from /proc/uptime)
    std::ifstream uptime_file("/proc/uptime");
    double        sys_uptime = 0.0;
    uptime_file >> sys_uptime;

    // process starttime in clock ticks since boot (starttime_jiffies)
    long   clk_tck = sysconf(_SC_CLK_TCK);
    double process_start_sec = starttime_jiffies / (double)clk_tck;
    uptime_sec = sys_uptime - process_start_sec;
    if (uptime_sec < 0)
        uptime_sec = 0; // in case of weirdness
}