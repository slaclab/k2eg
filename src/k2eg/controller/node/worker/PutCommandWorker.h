#ifndef K2EG_CONTROLLER_NODE_WORKER_PUTCOMMANDWORKER_H_
#define K2EG_CONTROLLER_NODE_WORKER_PUTCOMMANDWORKER_H_

#include <k2eg/common/types.h>
#include <k2eg/controller/node/worker/CommandWorker.h>
#include <k2eg/service/epics/EpicsServiceManager.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/pubsub/IPublisher.h>


namespace k2eg::controller::node::worker {

//Class that implements the put epics command
class PutCommandWorker : public CommandWorker {
    k2eg::service::log::ILoggerShrdPtr logger;
    k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager;
public:
    PutCommandWorker(k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager);
    virtual ~PutCommandWorker() = default;
    void processCommand(k2eg::controller::command::cmd::ConstCommandShrdPtr command);
};
}

#endif // K2EG_CONTROLLER_NODE_WORKER_PUTCOMMANDWORKER_H_