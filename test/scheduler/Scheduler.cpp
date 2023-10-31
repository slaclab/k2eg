#include <gtest/gtest.h>
#include <k2eg/service/scheduler/Scheduler.h>
#include <k2eg/service/scheduler/Task.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <memory>
#include <latch>

using namespace k2eg::service::scheduler;

TEST(Scheduler, StartStop) {
  Scheduler scheduler(std::make_unique<const SchedulerConfiguration>(SchedulerConfiguration{.thread_number = 1}));
  scheduler.start();
  sleep(2);
  scheduler.stop();
}

TEST(Scheduler, SchedulerSubmitAndExecuteEverySeconds) {
  int                   call_num        = 0;
  TaskHandlerFunction task_handler    = [&call_num](TaskProperties& properties) { call_num++; };
  TaskShrdPtr           task_shared_ptr = MakeTaskShrdPtr("task-1", "* * * * * *", task_handler);
  Scheduler             scheduler(std::make_unique<const SchedulerConfiguration>(SchedulerConfiguration{.thread_number = 1}));

  scheduler.start();
  scheduler.addTask(task_shared_ptr);
  sleep(5);
  scheduler.stop();
  ASSERT_NE(call_num, 0);
}

TEST(Scheduler, SubmitAndRemove) {
  std::mutex            tasks_queue_mtx;
  int                   call_num     = 0;
  TaskHandlerFunction task_handler = [&call_num, &tasks_queue_mtx](TaskProperties& properties) {
    std::lock_guard<std::mutex> lock(tasks_queue_mtx);
    call_num++;
  };
  TaskShrdPtr task_shared_ptr = MakeTaskShrdPtr("task-1", "*/1 * * * * *", task_handler);
  Scheduler   scheduler(std::make_unique<const SchedulerConfiguration>(SchedulerConfiguration{.thread_number = 1}));

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

TEST(Scheduler, SubmitAndRemoveLongRunnignTask) {
  std::latch work_done{1};
  TaskHandlerFunction task_handler = [&work_done](TaskProperties& task_properties) {
    work_done.count_down();
    sleep(10);
    work_done.max();
  };
  TaskShrdPtr task_shared_ptr = MakeTaskShrdPtr("task-1", "* * * * * *", task_handler);
  Scheduler   scheduler(std::make_unique<const SchedulerConfiguration>(SchedulerConfiguration{.thread_number = 1}));

  scheduler.start();
  scheduler.addTask(task_shared_ptr);
  work_done.wait();
  scheduler.removeTaskByName("task-1");
  scheduler.stop();
}