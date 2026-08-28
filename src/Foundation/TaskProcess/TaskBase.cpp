#include "Foundation/TaskProcess/TaskBase.h"

namespace PiTrac
{
TaskBase::TaskBase(const std::string &name)
    : name_(name)
    , task_id_(generateTaskId())
    , logger_(GSLogger::getInstance())
    , status_(TaskStatus::NotStarted)
    , should_stop_(false)
{
    logInfo("Task created: " + name_ + " [" + task_id_ + "]");
    MessagerBase::createContext();
}

TaskBase::~TaskBase()
{
    if(isRunning())
    {
        end();
    }
    MessagerBase::destroyContext();
    logInfo("Task destroyed: " + name_);
}

bool TaskBase::run()
{
    // Guard against running the task if it's already running
    if (getStatus() == TaskStatus::Running)
    {
        logWarning("Task already running: " + name_);
        return false;
    }
    changeStatus(TaskStatus::Starting);
    start_time_ = std::chrono::steady_clock::now();
    // Setup process environment
    if (!setupProcess())
    {
        throw std::runtime_error("Failed to setup process for task: " + name_);
        exit(1);
    }

    // Run main loop
    processMain();

    // Cleanup the task process environment and resources
    cleanupProcess();

    logInfo("Process exiting for task: " + name_);
    exit(0);

    return true;
}

void TaskBase::end()
{
    logInfo("Signal ending task: " + name_);

    if (!isRunning())
    {
        logWarning("Task not running, cannot end: " + name_);
        return;
    }

    changeStatus(TaskStatus::Stopping);

    should_stop_ = true;

    auto runtime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time_);

    logInfo("Task " + name_ + " execution completed. Runtime: " +
            std::to_string(runtime.count()) + "s, ");
}

void TaskBase::forceKill()
{
    logInfo("Force killing task: " + name_);
    exit(1);
}

bool TaskBase::setupProcess()
{
    // Start all event threads
    for(auto &thread : event_threads_)
    {
        thread->start();
    }
    changeStatus(TaskStatus::Running);
    return true;
}

void TaskBase::cleanupProcess()
{
    // Stop all event threads
    for(auto &thread : event_threads_)
    {
        thread->stop();
    }
    event_threads_.clear();
}
} // namespace PiTrac