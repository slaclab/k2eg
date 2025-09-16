#ifndef EpicsServiceManager_H
#define EpicsServiceManager_H

#include <k2eg/common/BS_thread_pool.hpp>
#include <k2eg/common/MsgpackSerialization.h>
#include <k2eg/common/ThrottlingManager.h>
#include <k2eg/common/broadcaster.h>
#include <k2eg/common/types.h>

#include <k2eg/service/epics/EpicsChannel.h>
#include <k2eg/service/epics/EpicsMonitorOperation.h>
#include <k2eg/service/epics/Serialization.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/metric/IMetricService.h>
#include <k2eg/service/scheduler/Scheduler.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>

namespace k2eg::service::epics_impl {

DEFINE_MAP_FOR_TYPE(std::string, EpicsChannelShrdPtr, EpicsChannelMap)
typedef const EventReceivedShrdPtr&                                 EpicsServiceManagerHandlerParamterType;
typedef std::function<void(EpicsServiceManagerHandlerParamterType)> EpicsServiceManagerHandler;

/*
Represent a PV with his name and field
*/
struct PV
{
    std::string protocol;
    std::string name;
    std::string field;
};
DEFINE_PTR_TYPES(PV)

struct PvRuntimeStats
{
    std::atomic<std::uint64_t> total_duration_ns{0};
    std::atomic<std::uint64_t> invocation_count{0};
    std::atomic<double>        last_backlog_count{0.0};
};
DEFINE_PTR_TYPES(PvRuntimeStats)

struct EpicsServiceManagerConfig
{
    // the number of thread for execute the poll
    // on the epics monitor operation
    std::int32_t thread_count = 1;

    // the maximum number of events to fetch from the monitor queue
    // this is used to limit the number of events to process
    std::int32_t max_event_from_monitor_queue = 100;

    // Per-PV throttling configuration (microseconds)
    std::int32_t pv_min_throttle_us = 50;    // minimal backoff for idle PVs
    std::int32_t pv_max_throttle_us = 50000; // cap backoff to 50ms
    std::int32_t pv_idle_threshold = 10;     // idle cycles before increasing backoff

    // Disable per-thread throttling updates/metrics, prefer per-PV
    bool disable_thread_throttle = true;
};

// describe a channel element in map per each PV
struct ChannelMapElement
{
    std::shared_ptr<EpicsChannel>                    channel;
    std::atomic<bool>                                to_force{false};
    std::atomic<int>                                 keep_alive{0};
    std::atomic<bool>                                active{false};
    std::shared_ptr<k2eg::common::ThrottlingManager> pv_throttle;
    std::shared_ptr<PvRuntimeStats>                  runtime_stats = std::make_shared<PvRuntimeStats>();
};
DEFINE_PTR_TYPES(ChannelMapElement)

struct ChannelTask
{
    ChannelMapElementShrdPtr         state;
    ConstMonitorOperationShrdPtr     monitor;
};
DEFINE_PTR_TYPES(ChannelTask)
typedef std::unique_lock<std::shared_mutex> WriteLockCM;
typedef std::shared_lock<std::shared_mutex> ReadLockCM;

DEFINE_PTR_TYPES(EpicsServiceManagerConfig)

DEFINE_MAP_FOR_TYPE(std::string, ChannelTaskShrdPtr, ChannelMap)

class EpicsServiceManager
{
    k2eg::service::log::ILoggerShrdPtr                                logger;
    ConstEpicsServiceManagerConfigUPtr                                config;
    std::shared_mutex                                                 channel_map_mutex;
    ChannelMap                                                        channel_map;
    std::set<std::string>                                             pv_to_remove;
    std::set<std::string>                                             pv_to_force;
    k2eg::common::broadcaster<EpicsServiceManagerHandlerParamterType> handler_broadcaster;
    std::unique_ptr<pvac::ClientProvider>                             pva_provider;
    std::unique_ptr<pvac::ClientProvider>                             ca_provider;
    bool                                                              end_processing;
    std::shared_ptr<BS::light_thread_pool>                            processing_pool;
    k2eg::service::metric::IEpicsMetric&                              metric;
    
    /*
    @brief task is the main function for the thread pool

    @param monitor_op the monitor operation to execute
    @details this function is called by the thread pool
    */
    void task(ChannelTaskShrdPtr task_entry);
    inline void recordTaskDuration(const std::chrono::steady_clock::time_point& start_time,
                                   const PvRuntimeStatsShrdPtr& pv_stats_ptr);
    inline void cleanupAndErase(const std::string& pv_name,
                                const ChannelTaskShrdPtr& task_entry,
                                const ChannelMapElementShrdPtr& state,
                                bool erase_from_map,
                                bool record_duration,
                                const std::chrono::steady_clock::time_point& start_time,
                                const PvRuntimeStatsShrdPtr& pv_stats_ptr);
    void handleStatistic(k2eg::service::scheduler::TaskProperties& task_properties);

public:
    explicit EpicsServiceManager(ConstEpicsServiceManagerConfigUPtr config = std::make_unique<EpicsServiceManagerConfig>());
    ~EpicsServiceManager();
    void                         addChannel(const std::string& pv_name_uri);
    void                         removeChannel(const std::string& pv_name_uri);
    void                         monitorChannel(const std::string& pv_identification, bool activate);
    void                         forceMonitorChannelUpdate(const std::string& pv_name, bool is_uri = true);
    ConstMonitorOperationShrdPtr getMonitorOp(const std::string& pv_name_uri);
    ConstGetOperationUPtr        getChannelData(const std::string& pv_name_uri);
    ConstPutOperationUPtr putChannelData(const std::string& pv_name, std::unique_ptr<k2eg::common::MsgpackObject> value);
    size_t getChannelMonitoredSize();
    /*
    give some sanitizaiton to epics pv names patter <pvname>.<value>
    this method always return a valid pointer if the name is totaly
    wrong and noting is matched the default value are returned.

    thi method recognize values using multi dot notaion for substructures
    so either the two form are valid:

    ioc:pv_name.value
    ioc:pv_name.field.subfield
    */
    PVUPtr sanitizePVName(const std::string& pv_name);
    /**
     * Register an event handler and return a token. Unitl this token is alive
     * the handler receives the events. The internal broadcaster dispose all handler
     * which token is invalid
     */
    k2eg::common::BroadcastToken                        addHandler(EpicsServiceManagerHandler new_handler);
    size_t                                              getHandlerSize();
    k2eg::common::StringVector getMonitoredChannels();
};

DEFINE_PTR_TYPES(EpicsServiceManager)

} // namespace k2eg::service::epics_impl
#endif
