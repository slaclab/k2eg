#ifndef k2eg_CONTROLLER_NODE_WORKER_COMMANDWORKER_H_
#define k2eg_CONTROLLER_NODE_WORKER_COMMANDWORKER_H_

#include <k2eg/common/BS_thread_pool.hpp>
#include <k2eg/controller/command/CMDCommand.h>
#include <k2eg/controller/node/worker/CommandWorker.h>

namespace k2eg::controller::node::worker {

class CommandWorker {
protected:
    std::shared_ptr<BS::thread_pool> shared_worker_processing;

public:
    CommandWorker(std::shared_ptr<BS::thread_pool> shared_worker_processing)
        : shared_worker_processing(shared_worker_processing){};
    CommandWorker() = delete;
    CommandWorker(const CommandWorker&) = delete;
    CommandWorker& operator=(const CommandWorker&) = delete;
    ~CommandWorker() = default;
    virtual bool submitCommand(k2eg::controller::command::CommandConstShrdPtr command) = 0;
};

} // namespace k2eg::controller::node::worker

#endif // k2eg_CONTROLLER_NODE_WORKER_COMMANDWORKER_H_