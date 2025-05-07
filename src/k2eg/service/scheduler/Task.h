#ifndef K2EG_SERVICE_SCHEDULER_TASK_H_
#define K2EG_SERVICE_SCHEDULER_TASK_H_

#include <k2eg/common/types.h>

#include <ctime>
#include <croncpp.h>
#include <functional>

namespace k2eg::service::scheduler {
class Scheduler;

#define RESCHEDULE_TIMEOUT 60 //seconds

struct TaskProperties {
    // let the scheduler run asap
    bool run_asap = false;
    // when a task set this field to true, it will be automatically removed
    // and no more executed
    bool completed = false;
};


typedef std::function<void(TaskProperties&)> TaskHandlerFunction;
/*
    Task that is executed on a specific crontab expression
*/
class Task {
    friend class Scheduler;
    const std::string name;
    TaskProperties task_properties;
    cron::cronexpr cron_expr;
    std::time_t next_schedule;
    TaskHandlerFunction handler;
    bool to_be_deleted = false;
 public:
    Task(const std::string& name, const std::string& cron_expr, TaskHandlerFunction handler, std::time_t starting_ts = std::time(0));
    void execute();
    bool canBeExecuted(const std::time_t& current_time);
    const std::string& getName();
    std::time_t nextSchedule();
    bool isCompleted() const;
};

DEFINE_PTR_TYPES(Task)

}  // namespace k2eg::service::scheduler

#endif  // K2EG_SERVICE_SCHEDULER_TASK_H_