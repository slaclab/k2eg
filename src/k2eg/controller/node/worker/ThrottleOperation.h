#ifndef K2EG_CONTROLLER_NODE_WORKER_THROTTLEOPERATION_H_
#define K2EG_CONTROLLER_NODE_WORKER_THROTTLEOPERATION_H_
#include <k2eg/controller/node/worker/CommandWorker.h>
namespace k2eg::controller::node::worker {
    class ThrottleOperation : public WorkerAsyncOperation {
        const std::chrono::milliseconds       throttle_time_ms;
        public:
         ThrottleOperation(std::uint32_t throttle_time_ms = 1)
             : WorkerAsyncOperation(std::chrono::milliseconds(0)), throttle_time_ms(std::chrono::milliseconds(throttle_time_ms)) {}
        void wait() {
            std::this_thread::sleep_for(throttle_time_ms);
        }
       };
}  // namespace k2eg::controller::node::worker

#endif // K2EG_CONTROLLER_NODE_WORKER_THROTTLEOPERATION_H_