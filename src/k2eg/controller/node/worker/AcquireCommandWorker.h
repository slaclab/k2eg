#ifndef k2eg_CONTROLLER_NODE_WORKER_ACQUIRECOMMANDWORKER_H_
#define k2eg_CONTROLLER_NODE_WORKER_ACQUIRECOMMANDWORKER_H_

#include <k2eg/common/types.h>
#include <k2eg/controller/node/worker/CommandWorker.h>
#include <k2eg/service/epics/EpicsServiceManager.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/pubsub/IPublisher.h>

#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>
namespace k2eg::controller::node::worker {

// Message structure for publisher
class MonitorMessage : public k2eg::service::pubsub::PublishMessage {
    const std::string request_type;
    k2eg::service::epics_impl::ConstMonitorEventShrdPtr monitor_event;
    const std::string queue;
    const std::string message;
public:
    MonitorMessage(const std::string& queue, k2eg::service::epics_impl::ConstMonitorEventShrdPtr monitor_event);
    virtual ~MonitorMessage() = default;
    char* getBufferPtr();
    size_t getBufferSize();
    const std::string& getQueue();
    const std::string& getDistributionKey();
    const std::string& getReqType();
};
// define the base ptr types
DEFINE_PTR_TYPES(MonitorMessage)

// map a channel to the topics where it need to be published
DEFINE_MAP_FOR_TYPE(std::string, std::vector<std::string>, ChannelTopicsMap);

//
// ss the command handler for the management of the AcquireCommand
//
class AcquireCommandWorker : public CommandWorker {
    mutable std::shared_mutex channel_map_mtx;
    ChannelTopicsMap channel_topics_map;
    k2eg::service::log::ILoggerShrdPtr logger;
    k2eg::service::pubsub::IPublisherShrdPtr publisher;
    k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager;
    // Handler's liveness token
    k2eg::common::BroadcastToken handler_token;

    void acquireManagement(k2eg::controller::command::CommandConstShrdPtr command);
    void epicsMonitorEvent(const k2eg::service::epics_impl::MonitorEventVecShrdPtr& event_data);

public:
    AcquireCommandWorker(std::shared_ptr<BS::thread_pool> shared_worker_processing,
                         k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager);
    virtual ~AcquireCommandWorker();
    bool submitCommand(k2eg::controller::command::CommandConstShrdPtr command);
};

} // namespace k2eg::controller::node::worker

#endif // k2eg_CONTROLLER_NODE_WORKER_ACQUIRECOMMANDWORKER_H_