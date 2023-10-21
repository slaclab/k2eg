#include <k2eg/service/scheduler/Scheduler.h>
#include <algorithm>
#include <chrono>
#include <iterator>
#include <mutex>
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

bool
Scheduler::removeTaskByName(const std::string& task_name) {
  int erased = 0;
  int retry = 3;
  // wait until the task is erased, sometime it cannot be erased because
  // is executing
  std::unique_lock<std::mutex> lock(tasks_queue_mtx);
  while(!erased) {
    // we can delete the task
    erased = std::erase_if(tasks_queue, [task_name](TaskShrdPtr& task) { return task->getName().compare(task_name) == 0; });
    if(!erased) {
      task_name_to_remove.insert(task_name);
      //wait for completion
      cv_remove_item.wait(lock);
    }
  }
  return erased;
}

void
Scheduler::scheduleTask() {
  while (processing) {
    std::time_t now      = std::time(0);
    TaskShrdPtr cur_task = nullptr;
    
    // protected block for find the task to proecess
    {
      std::unique_lock<std::mutex> lock(tasks_queue_mtx);
      if(tasks_queue.size()) {
        for (auto& task : tasks_queue) {
          if (task->canBeExecuted(now)) {
            cur_task = task;
            std::erase_if(tasks_queue, [task](TaskShrdPtr& checked_task) { return checked_task->getName().compare(task->getName()) == 0; });
            break;
          }
        }
      }
    }

    if (cur_task) {
      // execute the task
      cur_task->execute();
      {
        // lock the queue
        std::unique_lock<std::mutex> lock(tasks_queue_mtx);
        if(task_name_to_remove.contains(cur_task->name)) {
          // set task as to delete
          cur_task->to_be_deleted = true;
          // erase task name from the list of task to remove
          task_name_to_remove.erase(cur_task->name);
          //reinsert task into queue
          tasks_queue.push_back(cur_task);
        }
      }
      
      // notify pending removing waiting variables
      cv_remove_item.notify_all();
    } else{
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