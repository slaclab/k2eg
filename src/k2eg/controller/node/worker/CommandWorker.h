#ifndef k2eg_CONTROLLER_NODE_WORKER_COMMANDWORKER_H_
#define k2eg_CONTROLLER_NODE_WORKER_COMMANDWORKER_H_
#include <k2eg/common/types.h>
#include <k2eg/controller/command/CMDCommand.h>
#include <k2eg/controller/node/worker/CommandWorker.h>
#include <k2eg/common/BS_thread_pool.hpp>

#include <boost/json.hpp>
#include <chrono>
namespace k2eg::controller::node::worker {

class ReplySerializer {
  protected:
  std::shared_ptr<boost::json::object> reply_content; 
  k2eg::common::SerializationType ser_type;
public:
  ReplySerializer(std::shared_ptr<boost::json::object> reply_content, k2eg::common::SerializationType ser_type);
};

class WorkerAsyncOperation {
  const std::chrono::milliseconds       timeout_ms;
  std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

 public:
  WorkerAsyncOperation(std::chrono::milliseconds timeout_ms) : timeout_ms(timeout_ms) {}
  bool isTimeout() {
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - begin);
    return elapsed>timeout_ms;
  }
};

class CommandWorker {
 public:
  CommandWorker()                                                                          = default;
  CommandWorker(const CommandWorker&)                                                      = delete;
  CommandWorker& operator=(const CommandWorker&)                                           = delete;
  ~CommandWorker()                                                                         = default;
  virtual void processCommand(k2eg::controller::command::cmd::ConstCommandShrdPtr command) = 0;
};
DEFINE_PTR_TYPES(CommandWorker)
}  // namespace k2eg::controller::node::worker

#endif  // k2eg_CONTROLLER_NODE_WORKER_COMMANDWORKER_H_