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
  int retry = 0;
  int                   call_num        = 0;
  TaskHandlerFunction task_handler    = [&call_num](TaskProperties& properties) { call_num++; };
  TaskShrdPtr           task_shared_ptr = MakeTaskShrdPtr("task-1", "* * * * * *", task_handler);
  Scheduler             scheduler(std::make_unique<const SchedulerConfiguration>(SchedulerConfiguration{.thread_number = 1}));

  scheduler.start();
  sleep(2);
  scheduler.addTask(task_shared_ptr);
  while(call_num==0 && retry < 30) {
    sleep(1);
    retry++;
  }
  scheduler.stop();
  ASSERT_NE(call_num, 0);
}

TEST(Scheduler, SubmitAndRemove) {
  std::atomic_int32_t counter = 0;
  TaskHandlerFunction task_handler = [&counter](TaskProperties& properties) {
    counter++;
  };
  TaskShrdPtr task_shared_ptr = MakeTaskShrdPtr("task-1", "*/1 * * * * *", task_handler);
  Scheduler   scheduler(std::make_unique<const SchedulerConfiguration>(SchedulerConfiguration{
    .thread_number = 1,
    }));

  scheduler.start();
  scheduler.addTask(task_shared_ptr);
  while(counter==0) {
    sleep(1);
  }
  scheduler.removeTaskByName("task-1");
  counter = 0;
  sleep(5);
  scheduler.stop();
  ASSERT_EQ(counter, 0);
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