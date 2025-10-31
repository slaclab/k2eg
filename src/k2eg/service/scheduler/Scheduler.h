#ifndef K2EG_SERVICE_SCHEDULER_SCHEDULER_H_
#define K2EG_SERVICE_SCHEDULER_SCHEDULER_H_

#include <k2eg/service/scheduler/Task.h>

#include <croncpp.h>

#include <deque>
#include <mutex>
#include <thread>
#include <vector>
#include <sys/types.h>
#include <condition_variable>
namespace k2eg::service::scheduler {
typedef std::deque<TaskShrdPtr> TaskQueue;
typedef std::vector<TaskShrdPtr> TaskVector;
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
  ConstSchedulerConfigurationShrdPtr configuration;
  std::mutex                      tasks_queue_mtx;
  std::mutex                      thread_wait_mtx;
  std::condition_variable         cv;
  std::condition_variable         cv_remove_item;
  std::set<std::string>           task_name_to_remove;
  TaskQueue                       tasks_queue;
  TaskVector                      running_task_vector;
  std::vector<std::thread>        thread_group;
  std::vector<std::thread>        thread_group_cur_job;
  std::time_t                     time_to_wakeup;

  bool                                  processing;
  bool                                  stop_wait_var;
  void                                  scheduleTask(int thread_index);
  std::chrono::system_clock::time_point getNewWaitUntilTimePoint();

 public:
  Scheduler(ConstSchedulerConfigurationShrdPtr configuration);
  ~Scheduler();
  void start();
  void stop();
  void addTask(TaskShrdPtr task_shrd_ptr);
  bool removeTaskByName(const std::string& task_name);
};
DEFINE_PTR_TYPES(Scheduler)
}  // namespace k2eg::service::scheduler

#endif  // K2EG_SERVICE_SCHEDULER_SCHEDULER_H_
