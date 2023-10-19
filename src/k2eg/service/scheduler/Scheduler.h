#ifndef K2EG_SERVICE_SCHEDULER_SCHEDULER_H_
#define K2EG_SERVICE_SCHEDULER_SCHEDULER_H_

#include <croncpp.h>
#include <k2eg/service/scheduler/Task.h>
#include <sys/types.h>

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>

namespace k2eg::service::scheduler {
typedef std::deque<TaskShrdPtr> TaskQueue;
#define THREAD_SLEEP_SECONDS 60

struct SchedulerConfiguration {
    // wakeup thread after a specific amount of secondds
    unsigned int check_every_amount_of_seconds = THREAD_SLEEP_SECONDS;
    // the number of thread for the scheduler
    unsigned int thread_number;
};
DEFINE_PTR_TYPES(SchedulerConfiguration)

/*
    Permits to execute handler specifying cronjob timing string
*/
class Scheduler {
  ConstSchedulerConfigurationUPtr configuration;
  std::mutex               tasks_queue_mtx;
  std::mutex               thread_wait_mtx;
  std::condition_variable  cv;
  TaskQueue  tasks_queue;
  std::vector<std::thread> thread_group;
  std::time_t              time_to_wakeup;

  bool                     processing;
  bool                     new_taks_submitted;
  void                     scheduleTask();
  std::chrono::system_clock::time_point getNewWaitUntilTimePoint();  
 public:
  Scheduler(ConstSchedulerConfigurationUPtr configuration); 
  ~Scheduler() = default;
  void start();
  void stop();
  void addTask(TaskShrdPtr task_shrd_ptr);
  void removeTaskByName(const std::string& task_name);
};
DEFINE_PTR_TYPES(Scheduler)
}  // namespace k2eg::service::scheduler

#endif  // K2EG_SERVICE_SCHEDULER_SCHEDULER_H_