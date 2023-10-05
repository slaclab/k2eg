#include <k2eg/service/scheduler/Task.h>

using namespace k2eg::service::scheduler;

Task::Task(const std::string& name, const std::string& cron_expr, std::function<void()> handler):
name(name),
cron_expr(cron::make_cron(cron_expr)),
next_schedule(cron::cron_next(this->cron_expr, std::time(0))),
handler(handler){}

bool 
Task::canBeExecuted(const std::time_t& current_time) {
    return next_schedule <= current_time;
}

std::time_t 
Task::nextSchedule() {
    return next_schedule;
}

const std::string& 
Task::getName() {
    return name;
}

void 
Task::execute() {
    try{
        // execute  
        handler();
        // if all is gone ok calculate next timestamp for task
        next_schedule = cron::cron_next(this->cron_expr, std::time(0));
    } catch(...){
        //repeat form 60 second
       next_schedule = std::time(0)+RESCHEDULE_TIMEOUT;
    }
}