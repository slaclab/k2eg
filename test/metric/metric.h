#ifndef METRIC_H_
#define METRIC_H_

#include <curl/curl.h>

#include <iomanip>
#include <iostream>
#include <map>
#include <regex>
#include <string>

using namespace std;

#define RANDOM_PORT (unsigned int)(18080 + (rand() % 1000))
#define METRIC_URL_FROM_PORT(x) "http://localhost:"+std::to_string(x)+"/metrics"

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

static std::string getUrl(std::string url)
{
    CURL*       curl;
    CURLcode    res;
    std::string readBuffer;

    curl = curl_easy_init();
    if (curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }
    return readBuffer;
}

// Summarize system metrics from Prometheus metrics string
inline void printSystemMetricsTable(const std::string& metrics_string, bool print_header = false) {
    const std::vector<std::string> header = {
        "User mode ticks", "Kernel mode ticks", "CPU seconds (user+sys)", "IO read bytes", "IO write bytes",
        "Voluntary context switches", "Nonvoluntary context switches", "Resident Set Size (kB)",
        "Peak Resident Set Size (kB)", "Virtual Memory Size (kB)", "Data Segment Size (kB)", "Swap Size (kB)",
        "Number of threads", "Open file descriptors", "Max file descriptors", "Uptime (seconds)", "Start time (jiffies since boot)"
    };
    const std::vector<int> widths = {
        17, 18, 24, 17, 16, 27, 31, 24, 29, 25, 24, 16, 19, 23, 22, 17, 31
    };
    std::regex metric_regex(R"((k2eg_system_[a-z_]+)\{[^\}]*\}\s+([0-9.eE+-]+))");
    std::map<std::string, std::string> values;
    std::smatch match;
    std::string::const_iterator searchStart(metrics_string.cbegin());
    while (std::regex_search(searchStart, metrics_string.cend(), match, metric_regex)) {
        values[match[1]] = match[2];
        searchStart = match.suffix().first;
    }
    if (print_header) {
        // Print header
        for (size_t i = 0; i < header.size(); ++i)
            std::cout << "|" << std::setw(widths[i]) << header[i] << " ";
        std::cout << "|" << std::endl;
        // Print separator
        for (size_t i = 0; i < header.size(); ++i)
            std::cout << "|" << std::setw(widths[i]) << std::setfill('-') << "" ;
        std::cout << "|" << std::setfill(' ') << std::endl;
    }
    // Print values in order
    const std::vector<std::string> keys = {
        "k2eg_system_utime_ticks_total", "k2eg_system_stime_ticks_total", "k2eg_system_cpu_seconds_total",
        "k2eg_system_io_read_bytes_total", "k2eg_system_io_write_bytes_total",
        "k2eg_system_voluntary_ctxt_switches_total", "k2eg_system_nonvoluntary_ctxt_switches_total",
        "k2eg_system_vmrss_kb", "k2eg_system_vmhwm_kb", "k2eg_system_vmsize_kb", "k2eg_system_vmdata_kb",
        "k2eg_system_vmswap_kb", "k2eg_system_threads", "k2eg_system_open_fds", "k2eg_system_max_fds",
        "k2eg_system_uptime_seconds", "k2eg_system_starttime_jiffies"
    };
    for (size_t i = 0; i < keys.size(); ++i)
        std::cout << "|" << std::setw(widths[i]) << values[keys[i]] << " ";
    std::cout << "|" << std::endl;
}
#endif // METRIC_H_
