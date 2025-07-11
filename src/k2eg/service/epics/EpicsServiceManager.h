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

struct EpicsServiceManagerConfig
{
    // the number of thread for execute the poll
    // on the epics monitor operation
    std::int32_t thread_count = 1;

    // the maximum number of events to fetch from the monitor queue
    // this is used to limit the number of events to process
    std::int32_t max_event_from_monitor_queue = 100;
};

// describe a channel ellement in map per each PV
struct ChannelMapElement
{
    // this is the channel object
    std::shared_ptr<EpicsChannel> channel;
    // this is used to mark the channel as to be forced
    bool to_force;
    // this is used to mark the channel as to be removed when keep alive is 0
    int keep_alive;
    // Indicates if the channel is currently active
    bool active = false;
};
DEFINE_PTR_TYPES(ChannelMapElement)
typedef std::unique_lock<std::shared_mutex> WriteLockCM;
typedef std::unique_lock<std::shared_mutex> ReadLockCM;

DEFINE_PTR_TYPES(EpicsServiceManagerConfig)

DEFINE_MAP_FOR_TYPE(std::string, ChannelMapElement, ChannelMap)

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
    std::vector<k2eg::common::ThrottlingManager>                      thread_throttling_vector;
    mutable std::mutex                                                thread_throttle_mutex;
    /*
    @brief task is the main function for the thread pool

    @param monitor_op the monitor operation to execute
    @details this function is called by the thread pool
    */
    void task(ConstMonitorOperationShrdPtr monitor_op);
    void handleStatistic(k2eg::service::scheduler::TaskProperties& task_properties);

public:
    explicit EpicsServiceManager(ConstEpicsServiceManagerConfigUPtr config = std::make_unique<EpicsServiceManagerConfig>());
    ~EpicsServiceManager();
    void                         addChannel(const std::string& pv_name_uri);
    void                         removeChannel(const std::string& pv_name_uri);
    void                         monitorChannel(const std::string& pv_identification, bool activate);
    void                         forceMonitorChannelUpdate(const std::string& pv_name_uri);
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
    k2eg::common::StringVector                          getMonitoredChannels();
    const std::vector<k2eg::common::ThrottlingManager>& getThreadThrottlingInfo() const;
};

DEFINE_PTR_TYPES(EpicsServiceManager)

} // namespace k2eg::service::epics_impl
#endif