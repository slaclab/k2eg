#ifndef k2eg_CONTROLLER_NODE_WORKER_GETCOMMANDWORKER_H_
#define k2eg_CONTROLLER_NODE_WORKER_GETCOMMANDWORKER_H_

#include <k2eg/controller/node/worker/CommandWorker.h>
#include <k2eg/service/epics/EpicsServiceManager.h>

#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/pubsub/IPublisher.h>
namespace k2eg::controller::node::worker {

class GetMessage : public k2eg::service::pubsub::PublishMessage {
    const std::string request_type;
    const std::string destination_topic;
     k2eg::service::epics_impl::ConstChannelDataUPtr channel_data;
    const std::string message;
public:
    GetMessage(
        const std::string& destination_topic,
    k2eg::service::epics_impl::ConstChannelDataUPtr channel_data);
    virtual ~GetMessage() = default;
    char* getBufferPtr();
    size_t getBufferSize();
    const std::string& getQueue();
    const std::string& getDistributionKey();
    const std::string& getReqType();
};

class GetCommandWorker : public CommandWorker {
        k2eg::service::log::ILoggerShrdPtr logger;
    k2eg::service::pubsub::IPublisherShrdPtr publisher;
    k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager;
    void getOp(k2eg::controller::command::CommandConstShrdPtr command);

public:
    GetCommandWorker(std::shared_ptr<BS::thread_pool> shared_worker_processing,
                     k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager);
    virtual ~GetCommandWorker() = default;
    bool submitCommand(k2eg::controller::command::CommandConstShrdPtr command);
};

} // namespace k2eg::controller::node::worker

#endif // k2eg_CONTROLLER_NODE_WORKER_GETCOMMANDWORKER_H_