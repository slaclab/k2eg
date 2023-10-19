#include <k2eg/service/scheduler/Scheduler.h>
#include <chrono>
#include <iterator>
#include <ostream>
#include <iostream>
#include "k2eg/service/scheduler/Task.h"

using namespace k2eg::service::scheduler;
using namespace std::chrono;

Scheduler::Scheduler(ConstSchedulerConfigurationUPtr configuration):configuration(std::move(configuration)){}

void
Scheduler::start() {
  processing = true;
  for (int idx = 0; idx < configuration->thread_number; idx++) { thread_group.push_back(std::thread(&Scheduler::scheduleTask, this)); }
}
void
Scheduler::stop() {
  {
    std::lock_guard<std::mutex> lock(thread_wait_mtx);
    processing = false;
  }
  cv.notify_one();
  for (auto& t : thread_group) { t.join(); }
}

void
Scheduler::addTask(TaskShrdPtr task_shrd_ptr) {
  {
    std::lock_guard<std::mutex> lock(tasks_queue_mtx);
    tasks_queue.push_front(task_shrd_ptr);
    std::lock_guard<std::mutex> lock_for_Variable(thread_wait_mtx);
    new_taks_submitted = true;
  }
  // wakeup condition variable to update his wait_until value
  cv.notify_one();
}

void
Scheduler::removeTaskByName(const std::string& task_name) {
  int erased = 0;
  // wait until the task is erased, sometime it cannot be erased because
  // is executing
  while(!erased) {
    {
      std::lock_guard<std::mutex> lock(tasks_queue_mtx);
      erased = std::erase_if(tasks_queue, [task_name](TaskShrdPtr& task) { return task->getName().compare(task_name) == 0; });
    }
  }
}

void
Scheduler::scheduleTask() {
  while (processing) {
    std::time_t now      = std::time(0);
    TaskShrdPtr cur_task = nullptr;

    // protected block for find the task to proecess
    {
      std::lock_guard<std::mutex> lock(tasks_queue_mtx);
      for (auto& task : tasks_queue) {
        if (task->canBeExecuted(now)) {
          cur_task = task;
          std::erase_if(tasks_queue, [task](TaskShrdPtr& checked_task) { return checked_task->getName().compare(task->getName()) == 0; });
          break;
        }
      }
    }

    if (cur_task) {
      // execute the task
      cur_task->execute();

      // lock the queue
      std::lock_guard<std::mutex> lock(tasks_queue_mtx);

      // reinsert task at the front of the queue
      tasks_queue.push_front(cur_task);
    }else{
      // sleep for a while if we haven't new job to execute
      {
        std::unique_lock<std::mutex> lock(thread_wait_mtx);
        new_taks_submitted = false;
        // Wait for a specific amount of time or until processing variable is true
        cv.wait_for(lock, std::chrono::seconds(configuration->check_every_amount_of_seconds), [this] { return !processing || !new_taks_submitted; });
      }
    }
  }
}