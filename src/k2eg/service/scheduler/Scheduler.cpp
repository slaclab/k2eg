#include <k2eg/service/scheduler/Scheduler.h>
#include <k2eg/service/scheduler/Task.h>

#include <algorithm>
#include <chrono>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <set>

using namespace k2eg::service::scheduler;
using namespace std::chrono;

Scheduler::Scheduler(ConstSchedulerConfigurationUPtr configuration)
    : configuration(std::move(configuration)),
      running_task_vector(this->configuration->thread_number, nullptr),
      stop_wait_var(false),
      processing(false)
{
}

Scheduler::~Scheduler()
{
    stop();
}

void Scheduler::start()
{
    // Prime STL concurrency types to avoid libstdc++ static init races
    static bool primed = [] {
        std::mutex m;
        std::unique_lock<std::mutex> lk(m);
        std::condition_variable cv;
        cv.wait_for(lk, std::chrono::milliseconds(1));
        return true;
    }();

    {
        std::lock_guard<std::mutex> lock(thread_wait_mtx);
        processing = true;
        stop_wait_var = false;
    }
    for (int idx = 0; idx < configuration->thread_number; idx++)
    {
        thread_group.emplace_back(&Scheduler::scheduleTask, this, idx);
    }
}

void Scheduler::stop()
{
    {
        std::lock_guard<std::mutex> lock(thread_wait_mtx);
        if (!processing) return;
        processing = false;
        stop_wait_var = true;
    }
    cv.notify_all();
    for (auto& t : thread_group)
    {
        if (t.joinable())
            t.join();
    }
    thread_group.clear();
}

void Scheduler::addTask(TaskShrdPtr task_shrd_ptr)
{
    {
        std::lock_guard<std::mutex> lock(tasks_queue_mtx);
        tasks_queue.push_front(task_shrd_ptr);
    }
    {
        std::lock_guard<std::mutex> lock(thread_wait_mtx);
        stop_wait_var = true;
    }
    cv.notify_all();
}

bool Scheduler::removeTaskByName(const std::string& task_name)
{
    int erased = 0;
    // wait until the task is erased, sometimes it cannot be erased because it is executing
    std::unique_lock<std::mutex> lock(tasks_queue_mtx);
    while (!erased)
    {
        erased = std::erase_if(tasks_queue,
            [&task_name](const TaskShrdPtr& task)
            {
                return task->getName() == task_name;
            });
        if (!erased)
        {
            // check if the task is a running one
            auto find_iter = std::find_if(running_task_vector.begin(), running_task_vector.end(),
                [&task_name](const TaskShrdPtr& running_task)
                {
                    return running_task && running_task->getName() == task_name;
                });
            if (find_iter == running_task_vector.end())
            {
                // the task to remove is not running, so we can exit
                break;
            }
            task_name_to_remove.insert(task_name);
            cv_remove_item.wait(lock);
        }
        else
        {
            task_name_to_remove.erase(task_name);
        }
    }
    return erased;
}

void Scheduler::scheduleTask(int thread_index)
{
    while (true)
    {
        // Check processing under lock to avoid races
        {
            std::unique_lock<std::mutex> wait_lock(thread_wait_mtx);
            if (!processing)
                break;
        }

        std::time_t now = std::time(nullptr);

        // Find a task to process
        TaskShrdPtr task_to_run;
        {
            std::unique_lock<std::mutex> lock(tasks_queue_mtx);
            if (!tasks_queue.empty())
            {
                for (auto it = tasks_queue.begin(); it != tasks_queue.end(); ++it)
                {
                    if ((*it)->canBeExecuted(now))
                    {
                        task_to_run = *it;
                        running_task_vector[thread_index] = task_to_run;
                        tasks_queue.erase(it);
                        break;
                    }
                }
            }
        }

        if (task_to_run)
        {
            // Execute the task outside the lock
            task_to_run->execute();

            {
                std::unique_lock<std::mutex> lock(tasks_queue_mtx);
                if (task_to_run->isCompleted())
                {
                    running_task_vector[thread_index].reset();
                }
                else
                {
                    if (task_name_to_remove.contains(task_to_run->getName()))
                    {
                        task_to_run->to_be_deleted = true;
                    }
                    tasks_queue.push_back(std::move(task_to_run));
                    running_task_vector[thread_index].reset();
                }
            }
            cv_remove_item.notify_all();
        }
        else
        {
            // Sleep if no job to execute, but wake up if processing changes or stop_wait_var is set
            std::unique_lock<std::mutex> lock(thread_wait_mtx);
            cv.wait_for(lock, std::chrono::seconds(configuration->check_every_amount_of_seconds),
                        [this] { return !processing || stop_wait_var; });
            stop_wait_var = false;
            if (!processing)
                break;
        }
    }
}