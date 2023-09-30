#ifndef K2EG_SERVICE_SCHEDULER_SCHEDULER_H_
#define K2EG_SERVICE_SCHEDULER_SCHEDULER_H_

#include<croncpp.h>
#include <k2eg/service/scheduler/Task.h>

#include <deque>
#include <mutex>
#include <thread>
#include <condition_variable>

namespace k2eg::service::scheduler {

#define THREAD_SLEEP_SECONDS 1
/*
    Permits to execute handler specifying cronjob timing string
*/
class Scheduler {
    std::mutex tasks_queue_mtx;
    std::mutex thread_wait_mtx;
    std::condition_variable cv;
    std::deque<TaskShrdPtr> tasks_queue;
    std::vector<std::thread> thread_group;
    bool processing; 
    void scheduleTask();
    public:
    void start(int thread_number);
    void stop();
    void addTask(TaskShrdPtr task_shrd_ptr);
    void removeTaskByName(std::string& task_name);
};

}  // namespace k2eg::service::scheduler

#endif  // K2EG_SERVICE_SCHEDULER_SCHEDULER_H_