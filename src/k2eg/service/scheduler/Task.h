#ifndef K2EG_SERVICE_SCHEDULER_TASK_H_
#define K2EG_SERVICE_SCHEDULER_TASK_H_

#include <ctime>
#include <croncpp.h>
#include <functional>

#include <k2eg/common/types.h>

namespace k2eg::service::scheduler {
class Scheduler;

#define RESCHEDULE_TIMEOUT 60 //seconds
/*
    Task that is executed on a specific crontab expression
*/
class Task {
    const std::string name;
    cron::cronexpr cron_expr;
    std::time_t next_schedule;
    std::function<void()> handler;
 public:
    Task(const std::string& name, const std::string& cron_expr, std::function<void()> handler);
    void execute();
    bool canBeExecuted(const std::time_t& current_time);
    const std::string& getName();
    std::time_t nextSchedule();
};

DEFINE_PTR_TYPES(Task)

}  // namespace k2eg::service::scheduler

#endif  // K2EG_SERVICE_SCHEDULER_TASK_H_