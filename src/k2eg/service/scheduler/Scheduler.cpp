#include <k2eg/service/scheduler/Scheduler.h>

#include "k2eg/service/scheduler/Task.h"

using namespace k2eg::service::scheduler;

void
Scheduler::start(int thread_number) {
  processing = true;
  for (int idx = 0; idx < thread_number; idx++) { thread_group.push_back(std::thread(&Scheduler::scheduleTask, this)); }
}
void
Scheduler::stop() {
  processing = false;
  for (auto& t : thread_group) {}
}

void
Scheduler::addTask(TaskShrdPtr task_shrd_ptr) {
  std::lock_guard<std::mutex> lock(tasks_queue_mtx);
  tasks_queue.push_front(task_shrd_ptr);
}

void
Scheduler::removeTaskByName(std::string& task_name) {
  std::lock_guard<std::mutex> lock(tasks_queue_mtx);
  std::erase_if(tasks_queue, [task_name](TaskShrdPtr& task) { return task->name.compare(task_name) == 0; });
}

void
Scheduler::scheduleTask() {
  while (processing) {
    std::time_t now      = std::time_t();
    TaskShrdPtr cur_task = nullptr;

    // protcted block for find the task to proecess
    {
      std::lock_guard<std::mutex> lock(tasks_queue_mtx);
      for (auto& task : tasks_queue) {
        if (task->canBeExecuted(now)) {
          std::erase_if(tasks_queue, [task](TaskShrdPtr& checked_task) { return checked_task->name.compare(task->name) == 0; });
          cur_task = task;
          break;
        }
      }
    }

    if (cur_task) {
      // execute the task
      cur_task->handler();

      // lock the queue
      std::lock_guard<std::mutex> lock(tasks_queue_mtx);

      // reinsert task at the front of the queue
      tasks_queue.push_front(cur_task);
    }

    // sleep for a while
    {
      std::unique_lock<std::mutex> lock(thread_wait_mtx);
      cv.wait_for(lock, std::chrono::seconds(THREAD_SLEEP_SECONDS), [this] { return !processing; });  // Wait for the flag to become true
    }
  }
}