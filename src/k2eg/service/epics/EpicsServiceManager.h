#ifndef EpicsServiceManager_H
#define EpicsServiceManager_H
#include <k2eg/common/types.h>
#include <k2eg/common/broadcaster.h>
#include <k2eg/service/epics/EpicsChannel.h>
#include <k2eg/service/epics/Serialization.h>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace k2eg::service::epics_impl {

DEFINE_MAP_FOR_TYPE(std::string, EpicsChannelShrdPtr, EpicsChannelMap)
typedef const MonitorEventVecShrdPtr& EpicsServiceManagerHandlerParamterType;
typedef std::function<void(EpicsServiceManagerHandlerParamterType)> EpicsServiceManagerHandler;

class EpicsServiceManager {
    std::mutex channel_map_mutex;
    std::map<std::string, std::shared_ptr<EpicsChannel>> channel_map;
    std::unique_ptr<std::thread> scheduler_thread;
    k2eg::common::broadcaster<EpicsServiceManagerHandlerParamterType> handler_broadcaster;
    std::unique_ptr<pvac::ClientProvider> pva_provider;
    std::unique_ptr<pvac::ClientProvider> ca_provider;
    bool run = false;
    void task();
    void processIterator(const std::shared_ptr<EpicsChannel>& epics_channel);

public:
    explicit EpicsServiceManager();
    ~EpicsServiceManager();
    void addChannel(const std::string& channel_name, const std::string& protocol = "pva");
    void removeChannel(const std::string& channel_name);
    void monitorChannel(const std::string& channel_name, bool activate, const std::string& protocol);
    ConstChannelDataUPtr getChannelData(const std::string& channel_name, const std::string& protocol = "pva");
    ConstPutOperationUPtr putChannelData(const std::string& channel_name, const std::string& field, const std::string& value, const std::string& protocol = "pva");
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