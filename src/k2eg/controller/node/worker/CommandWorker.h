#ifndef k2eg_CONTROLLER_NODE_WORKER_COMMANDWORKER_H_
#define k2eg_CONTROLLER_NODE_WORKER_COMMANDWORKER_H_
#include <k2eg/common/types.h>
#include <k2eg/common/BS_thread_pool.hpp>
#include <k2eg/controller/command/CMDCommand.h>
#include <k2eg/controller/node/worker/CommandWorker.h>

namespace k2eg::controller::node::worker {

class CommandWorker {
public:
    CommandWorker() = default;
    CommandWorker(const CommandWorker&) = delete;
    CommandWorker& operator=(const CommandWorker&) = delete;
    ~CommandWorker() = default;
    virtual void processCommand(k2eg::controller::command::cmd::ConstCommandShrdPtr command) = 0;
};
DEFINE_PTR_TYPES(CommandWorker)
} // namespace k2eg::controller::node::worker

#endif // k2eg_CONTROLLER_NODE_WORKER_COMMANDWORKER_H_