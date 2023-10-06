#include <gtest/gtest.h>
#include <k2eg/service/scheduler/Scheduler.h>
#include <sys/wait.h>

#include <memory>

#include <k2eg/service/scheduler/Task.h>

using namespace k2eg::service::scheduler;

TEST(Scheduler, StartStop) {
  Scheduler             scheduler(std::make_unique<const SchedulerConfiguration>(SchedulerConfiguration{.thread_number=1}));
  scheduler.start();
  sleep(2);
  scheduler.stop();
}

TEST(Scheduler, SchedulerSubmitAndExecuteEverySeconds) {
  int                   call_num        = 0;
  std::function<void()> task_handler    = [&call_num]() { 
    call_num++; 
    };
  TaskShrdPtr           task_shared_ptr = MakeTaskShrdPtr("task-1", "* * * * * *", task_handler);
  Scheduler             scheduler(std::make_unique<const SchedulerConfiguration>(SchedulerConfiguration{.thread_number=1}));

  scheduler.start();
  scheduler.addTask(task_shared_ptr);
  sleep(5);
  scheduler.stop();
  ASSERT_NE(call_num, 0);
}

TEST(Scheduler, SubmitAndShoutdownWithoutExecuting) {
  int                   call_num        = 0;
  std::function<void()> task_handler    = [&call_num]() { 
    call_num++; 
    };
  TaskShrdPtr           task_shared_ptr = MakeTaskShrdPtr("task-1", "*/30 * * * * *", task_handler);
  Scheduler             scheduler(std::make_unique<const SchedulerConfiguration>(SchedulerConfiguration{.thread_number=1}));

  scheduler.start();
  scheduler.addTask(task_shared_ptr);
  sleep(5);
  scheduler.stop();
  ASSERT_EQ(call_num, 0);
}

TEST(Scheduler, SubmitAndRemove) {
  std::mutex            tasks_queue_mtx;
  int                   call_num        = 0;
  std::function<void()> task_handler    = [&call_num, &tasks_queue_mtx]() { 
    std::lock_guard<std::mutex> lock(tasks_queue_mtx);
    call_num++; 
    };
  TaskShrdPtr           task_shared_ptr = MakeTaskShrdPtr("task-1", "*/1 * * * * *", task_handler);
  Scheduler             scheduler(std::make_unique<const SchedulerConfiguration>(SchedulerConfiguration{.thread_number=1}));

  scheduler.start();
  scheduler.addTask(task_shared_ptr);
  sleep(5);
  ASSERT_NE(call_num, 0);
  scheduler.removeTaskByName("task-1");
  {
    std::lock_guard<std::mutex> lock(tasks_queue_mtx);
    call_num = 0;
  }

  sleep(5);
  scheduler.stop();
  ASSERT_EQ(call_num, 0);
}