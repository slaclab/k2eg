#ifndef k2eg_CONTROLLER_NODE_WORKER_WORKERRESOLVER_H_
#define k2eg_CONTROLLER_NODE_WORKER_WORKERRESOLVER_H_

#include <map>
#include <string>
#include <memory>
#include <k2eg/controller/command/CMDCommand.h>
namespace k2eg::controller::node::worker
{
    template <typename T>
    class WorkerResolver
    {
        std::map<k2eg::controller::command::CommandType, std::shared_ptr<T>> typed_instances;
    public:
        void registerService(const k2eg::controller::command::CommandType type, std::shared_ptr<T> object)
        {
            typed_instances.insert(std::pair<k2eg::controller::command::CommandType, std::shared_ptr<T>>(type, object));
        }
        std::shared_ptr<T> resolve(const k2eg::controller::command::CommandType type)
        {
            return typed_instances[type];
        }
    };
}

#endif // k2eg_CONTROLLER_NODE_WORKER_WORKERRESOLVER_H_