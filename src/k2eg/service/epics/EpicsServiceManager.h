#ifndef EpicsServiceManager_H
#define EpicsServiceManager_H

#include <k2eg/common/types.h>
#include <k2eg/common/broadcaster.h>
#include <k2eg/service/epics/EpicsChannel.h>
#include <k2eg/service/epics/Serialization.h>
#include <cstdint>
#include <k2eg/common/BS_thread_pool.hpp>

#include <functional>
#include <map>
#include <memory>
#include <shared_mutex>
#include <thread>
#include <vector>
#include <queue>
#include "k2eg/service/epics/EpicsMonitorOperation.h"


namespace k2eg::service::epics_impl {

DEFINE_MAP_FOR_TYPE(std::string, EpicsChannelShrdPtr, EpicsChannelMap)
typedef const EventReceivedShrdPtr& EpicsServiceManagerHandlerParamterType;
typedef std::function<void(EpicsServiceManagerHandlerParamterType)> EpicsServiceManagerHandler;

/*
Represent a PV with his name and field
*/
struct PV {
    std::string protocol;
    std::string name;
    std::string field;
};
DEFINE_PTR_TYPES(PV)

struct EpicsServiceManagerConfig{
    // the number of thread for execute the poll 
    // on the epics monitor operation
    std::int32_t thread_count = 1;
};
DEFINE_PTR_TYPES(EpicsServiceManagerConfig)
class EpicsServiceManager {
    ConstEpicsServiceManagerConfigUPtr config;
    std::mutex channel_map_mutex;
    std::map<std::string, std::shared_ptr<EpicsChannel>> channel_map;
    k2eg::common::broadcaster<EpicsServiceManagerHandlerParamterType> handler_broadcaster;
    std::unique_ptr<pvac::ClientProvider> pva_provider;
    std::unique_ptr<pvac::ClientProvider> ca_provider;
    bool end_processing;
    BS::thread_pool processing_pool;
    // monitor handler queue
    std::mutex monitor_op_queue_mutx;
    std::set<std::string> pv_to_remove;
    void task(ConstMonitorOperationShrdPtr monitor_op);
public:
    explicit EpicsServiceManager(ConstEpicsServiceManagerConfigUPtr config = std::make_unique<EpicsServiceManagerConfig>());
    ~EpicsServiceManager();
    void addChannel(const std::string& pv_name_uri);
    void removeChannel(const std::string& pv_name);
    void monitorChannel(const std::string& pv_identification, bool activate);
    void forceMonitorChannelUpdate(const std::string& pv_name);
    ConstGetOperationUPtr getChannelData(const std::string& pv_name_uri);
    ConstPutOperationUPtr putChannelData(const std::string& pv_name, const std::string& value);
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
    k2eg::common::BroadcastToken addHandler(EpicsServiceManagerHandler new_handler);
    size_t getHandlerSize();
    k2eg::common::StringVector getMonitoredChannels();
};

DEFINE_PTR_TYPES(EpicsServiceManager)

} // namespace k2eg::service::epics_impl
#endif