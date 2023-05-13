#ifndef EpicsServiceManager_H
#define EpicsServiceManager_H

#include <k2eg/common/types.h>
#include <k2eg/common/broadcaster.h>
#include <k2eg/service/epics/EpicsChannel.h>
#include <k2eg/service/epics/Serialization.h>
#include <k2eg/common/BS_thread_pool.hpp>

#include <functional>
#include <map>
#include <memory>
#include <shared_mutex>
#include <thread>
#include <vector>
#include <queue>


namespace k2eg::service::epics_impl {

DEFINE_MAP_FOR_TYPE(std::string, EpicsChannelShrdPtr, EpicsChannelMap)
typedef const EventReceivedShrdPtr& EpicsServiceManagerHandlerParamterType;
typedef std::function<void(EpicsServiceManagerHandlerParamterType)> EpicsServiceManagerHandler;
//typedef std::function<void(EventType, EpicsServiceManagerHandlerParamterType)> EpicsServiceManagerHandler;

class EpicsServiceManager {
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
    explicit EpicsServiceManager();
    ~EpicsServiceManager();
    void addChannel(const std::string& pv_name, const std::string& protocol = "pva");
    void removeChannel(const std::string& pv_name);
    void monitorChannel(const std::string& pv_name, bool activate, const std::string& protocol);
    ConstGetOperationUPtr getChannelData(const std::string& pv_name, const std::string& protocol = "pva");
    ConstPutOperationUPtr putChannelData(const std::string& pv_name, const std::string& field, const std::string& value, const std::string& protocol = "pva");
    size_t getChannelMonitoredSize();
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