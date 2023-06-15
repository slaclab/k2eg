#ifndef K2EG_CONTROLLER_NODE_WORKER_PUTCOMMANDWORKER_H_
#define K2EG_CONTROLLER_NODE_WORKER_PUTCOMMANDWORKER_H_

#include <k2eg/common/types.h>
#include <k2eg/controller/node/worker/CommandWorker.h>
#include <k2eg/service/epics/EpicsServiceManager.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/pubsub/IPublisher.h>
#include <k2eg/service/metric/IMetricService.h>

#include <k2eg/common/BS_thread_pool.hpp>
#include <string>

#include "k2eg/service/epics/EpicsPutOperation.h"

namespace k2eg::controller::node::worker {


struct PutOpInfo : public WorkerAsyncOperation {
  std::string                                pv_name;
  std::string                                value;
  std::string                                reply_id;
  service::epics_impl::ConstPutOperationUPtr op;
  PutOpInfo(
    const std::string& pv_name, 
    const std::string& value, 
    const std::string& reply_id, 
    service::epics_impl::ConstPutOperationUPtr op,
    std::uint32_t tout_msc = 10000)
      : WorkerAsyncOperation(std::chrono::milliseconds(tout_msc))
      , pv_name(pv_name)
      , value(value)
      , reply_id(reply_id)
      , op(std::move(op)) {}
};
DEFINE_PTR_TYPES(PutOpInfo)

// Class that implements the put epics command
class PutCommandWorker : public CommandWorker {
  std::shared_ptr<BS::thread_pool>                      processing_pool;
  k2eg::service::log::ILoggerShrdPtr                    logger;
  k2eg::service::pubsub::IPublisherShrdPtr              publisher;
  k2eg::service::metric::IEpicsMetric&                  metric;
  k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager;
  void                                                  checkPutCompletion(PutOpInfoShrdPtr put_info);
  k2eg::common::ConstSerializedMessageShrdPtr           getReply(PutOpInfoShrdPtr put_info);
 public:
  PutCommandWorker(k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager);
  virtual ~PutCommandWorker();
  void processCommand(k2eg::controller::command::cmd::ConstCommandShrdPtr command);
};
}  // namespace k2eg::controller::node::worker

#endif  // K2EG_CONTROLLER_NODE_WORKER_PUTCOMMANDWORKER_H_