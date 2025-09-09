#include "Infrastructure/TaskProcess/GSTaskBase.h"
#include "GSTaskBase.h"
#include <sys/prctl.h>
#include <errno.h>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <random>

namespace PiTrac
{
GSTaskBase::GSTaskBase(const std::string &name)
    : task_name_(name)
    , task_id_(generateTaskId())
    , status_(TaskStatus::NotStarted)
    , should_stop_(false)
    , logger_(GSLogger::getInstance())
    , ipc_endpoint_("ipc://gs_task")
{
    logInfo("Task created: " + task_name_ + " [" + task_id_ + "]");
}

GSTaskBase::~GSTaskBase()
{
    if (isRunning())
    {
        stop();
    }
    logInfo("Task destroyed: " + task_name_);
}

bool GSTaskBase::start()
{
    if (status_ == TaskStatus::Running)
    {
        logWarning("Task already running: " + task_name_);
        return false;
    }

    logInfo("Starting task: " + task_name_);
    changeStatus(TaskStatus::Starting);

    // Pre-start hook, do any necessary setup before starting the process
    if (!preStartHook())
    {
        logError("Pre-start hook failed");
        changeStatus(TaskStatus::Failed);
        return false;
    }

    start_time_ = std::chrono::steady_clock::now();
    processEntryPoint();

    return true;
}

void GSTaskBase::stop()
{
    logInfo("Stopping task: " + task_name_);

    if (!isRunning())
    {
        logWarning("Task not running, cannot stop: " + task_name_);
        return;
    }
    preStopHook();

    changeStatus(TaskStatus::Stopping);

    should_stop_ = true;

    postStopHook();
}

void GSTaskBase::forceKill()
{
    logInfo("Force killing task: " + task_name_);
    exit(1);
}

TaskStatus GSTaskBase::getStatus() const
{
    return status_;
}

bool GSTaskBase::isRunning() const
{
    return status_ == TaskStatus::Running;
}

void GSTaskBase::processEntryPoint()
{
    // Set process name
    prctl(PR_SET_NAME, task_name_.c_str(), 0, 0, 0);

    logInfo("Process started for task: " + task_name_);

    try {
        // Setup child process environment
        if (!setupProcess())
        {
            logError("Failed to setup process");
            exit(1);
        }

        // Run main loop
        processMain();
    } catch (const std::exception &e) {
        logError("Exception in process main for task: " + task_name_ + " - " +
                 std::string(e.what()));
        exit(1);
    } catch (...) {
        logError("Unknown exception in process main for task: " + task_name_);
        exit(1);
    }

    // Cleanup
    cleanupProcess();

    logInfo("Process exiting for task: " + task_name_);
    exit(0);
}

void GSTaskBase::changeStatus(TaskStatus new_status)
{
    TaskStatus old_status = status_;

    old_status = status_;

    if (status_change_callback_)
    {
        status_change_callback_(new_status);
    }

    if (old_status != new_status)
    {
        status_ = new_status;
        logger_->info("[" + task_name_ + "] Task status changed: " +
                      taskStatusToString(old_status) +
                      " -> " + taskStatusToString(new_status));
    }
}

void GSTaskBase::logInfo(const std::string &message)
{
    if (logger_)
    {
        logger_->info("[" + task_name_ + "] " + message);
    }
}

void GSTaskBase::logWarning(const std::string &message)
{
    if (logger_)
    {
        logger_->warning("[" + task_name_ + "] " + message);
    }
}

void GSTaskBase::logError(const std::string &message)
{
    if (logger_)
    {
        logger_->error("[" + task_name_ + "] " + message);
    }
}

std::string GSTaskBase::generateTaskId()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1000, 9999);

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << task_name_ << "_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S")
       << "_" << dis(gen);

    return ss.str();
}
} // namespace PiTrac