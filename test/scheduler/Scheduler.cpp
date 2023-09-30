#include <gtest/gtest.h>
#include <k2eg/service/scheduler/Scheduler.h>
#include <sys/wait.h>

#include <memory>

#include <k2eg/service/scheduler/Task.h>

using namespace k2eg::service::scheduler;

TEST(Scheduler, SchedulerSubmitAndExecute) {
  int                   call_num        = 0;
  std::function<void()> task_handler    = [&call_num]() { 
    call_num++; 
    };
  TaskShrdPtr           task_shared_ptr = MakeTaskShrdPtr("task-1", "* * * * * *", task_handler);
  Scheduler             scheduler;

  scheduler.start(1);
  scheduler.addTask(task_shared_ptr);
  sleep(10);
  scheduler.stop();
  ASSERT_NE(call_num, 0);
}