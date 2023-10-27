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

Scheduler::Scheduler(ConstSchedulerConfigurationUPtr configuration):
configuration(std::move(configuration)),
running_task_vector(this->configuration->thread_number, nullptr){}

void
Scheduler::start() {
  processing = true;
  for (int idx = 0; idx < configuration->thread_number; idx++) { 
    thread_group.push_back(std::thread(&Scheduler::scheduleTask, this, idx)); 
  }
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
    // if the rpocesisng is not active
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
    erased = std::erase_if(tasks_queue, [&task_name](const TaskShrdPtr& task) { return task->getName().compare(task_name) == 0; });
    if(!erased) {
      // check if the task is a running one
      auto find_iter = std::find_if(running_task_vector.begin(), running_task_vector.end(), [&task_name](const TaskShrdPtr& running_task) { return running_task->getName().compare(task_name) == 0; });
      if(find_iter == running_task_vector.end()) {
        // the task tahat is going to remove is not a one that is still running
        // so we can go out
        break;
      }
      task_name_to_remove.insert(task_name);
      //wait for completion
      cv_remove_item.wait(lock);
    } else {
      // erase task name from the list of task to remove
      task_name_to_remove.erase(task_name);
    }
  }
  return erased;
}

void
Scheduler::scheduleTask(int thread_index) {
  while (processing) {
    std::time_t now      = std::time(0);
    
    // protected block for find the task to proecess
    {
      std::unique_lock<std::mutex> lock(tasks_queue_mtx);
      if(tasks_queue.size()) {
        for (auto& task : tasks_queue) {
          if (task->canBeExecuted(now)) {
            running_task_vector[thread_index] = task;
            std::erase_if(tasks_queue, [task](TaskShrdPtr& checked_task) { return checked_task->getName().compare(task->getName()) == 0; });
            break;
          }
        }
      }
    }

    if (running_task_vector[thread_index]) {
      // execute the task
      running_task_vector[thread_index]->execute();
      {
        // lock the queue
        std::unique_lock<std::mutex> lock(tasks_queue_mtx);
        if(task_name_to_remove.contains(running_task_vector[thread_index]->name)) {
          // set task as to delete
          running_task_vector[thread_index]->to_be_deleted = true;
        }
        // reinsert task into queue moving the shared pointer
        tasks_queue.push_back(std::move(running_task_vector[thread_index]));
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