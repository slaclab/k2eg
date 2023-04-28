#ifndef k2eg_CONTROLLER_NODE_WORKER_GETCOMMANDWORKER_H_
#define k2eg_CONTROLLER_NODE_WORKER_GETCOMMANDWORKER_H_

#include <k2eg/controller/command/cmd/Command.h>
#include <k2eg/controller/node/worker/CommandWorker.h>
#include <k2eg/service/epics/EpicsGetOperation.h>
#include <k2eg/service/epics/EpicsServiceManager.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/metric/IMetricService.h>
#include <k2eg/service/pubsub/IPublisher.h>

#include <chrono>
#include <k2eg/common/BS_thread_pool.hpp>
namespace k2eg::controller::node::worker {

class GetMessage : public k2eg::service::pubsub::PublishMessage {
  const std::string                                              request_type;
  const std::string                                              destination_topic;
  k2eg::service::epics_impl::ConstChannelDataUPtr                channel_data;
  const k2eg::service::epics_impl::ConstSerializedMessageShrdPtr message;

 public:
  GetMessage(const std::string&                                       destination_topic,
             k2eg::service::epics_impl::ConstChannelDataUPtr          channel_data,
             k2eg::service::epics_impl::ConstSerializedMessageShrdPtr message);
  virtual ~GetMessage() = default;
  char*              getBufferPtr();
  const size_t       getBufferSize();
  const std::string& getQueue();
  const std::string& getDistributionKey();
  const std::string& getReqType();
};

class GetOpInfo : public WorkerAsyncOperation {
 public:
  std::string                                pv_name;
  std::string                                destination_topic;
  command::cmd::MessageSerType               serialization;
  service::epics_impl::ConstGetOperationUPtr op;
  GetOpInfo(const std::string&                         pv_name,
            const std::string&                         destination_topic,
            const command::cmd::MessageSerType&        serialization,
            service::epics_impl::ConstGetOperationUPtr op)
      : WorkerAsyncOperation(std::chrono::milliseconds(3000)),
        pv_name(pv_name),
        destination_topic(destination_topic),
        serialization(serialization),
        op(std::move(op)) {}
};
DEFINE_PTR_TYPES(GetOpInfo)

class GetCommandWorker : public CommandWorker {
  std::shared_ptr<BS::thread_pool>                      processing_pool;
  k2eg::service::log::ILoggerShrdPtr                    logger;
  k2eg::service::pubsub::IPublisherShrdPtr              publisher;
  k2eg::service::metric::IEpicsMetric&                  metric;
  k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager;
  void                                                  checkGetCompletion(GetOpInfoShrdPtr put_info);

 public:
  GetCommandWorker(k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager);
  virtual ~GetCommandWorker();
  void processCommand(k2eg::controller::command::cmd::ConstCommandShrdPtr command);
};

}  // namespace k2eg::controller::node::worker

#endif  // k2eg_CONTROLLER_NODE_WORKER_GETCOMMANDWORKER_H_