#ifndef K2EG_CONTROLLER_NODE_WORKER_PUTCOMMANDWORKER_H_
#define K2EG_CONTROLLER_NODE_WORKER_PUTCOMMANDWORKER_H_

#include <k2eg/common/types.h>
#include <k2eg/controller/node/worker/CommandWorker.h>
#include <k2eg/service/epics/EpicsServiceManager.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/pubsub/IPublisher.h>
#include <k2eg/service/metric/IMetricService.h>

#include <k2eg/common/BS_thread_pool.hpp>

#include "k2eg/service/epics/EpicsPutOperation.h"

namespace k2eg::controller::node::worker {

struct PutOpInfo : public WorkerAsyncOperation {
  std::string                                channel_name;
  std::string                                value;
  service::epics_impl::ConstPutOperationUPtr op;
  PutOpInfo(
    const std::string& channel_name, 
    const std::string& value, 
    service::epics_impl::ConstPutOperationUPtr op)
      : WorkerAsyncOperation(std::chrono::milliseconds(3000))
      , channel_name(channel_name)
      , value(value)
      , op(std::move(op)) {}
};
DEFINE_PTR_TYPES(PutOpInfo)

// Class that implements the put epics command
class PutCommandWorker : public CommandWorker {
  std::shared_ptr<BS::thread_pool>                      processing_pool;
  k2eg::service::log::ILoggerShrdPtr                    logger;
    k2eg::service::metric::IEpicsMetric&                  metric;
  k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager;
  void                                                  checkPutCompletion(PutOpInfoShrdPtr put_info);

 public:
  PutCommandWorker(k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager);
  virtual ~PutCommandWorker();
  void processCommand(k2eg::controller::command::cmd::ConstCommandShrdPtr command);
};
}  // namespace k2eg::controller::node::worker

#endif  // K2EG_CONTROLLER_NODE_WORKER_PUTCOMMANDWORKER_H_