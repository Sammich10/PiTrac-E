#ifndef GSTASKBASE_H
#define GSTASKBASE_H

#include "Common/Utils/Logging/GSLogger.h"
#include "Common/System/System.h"
#include "Common/System/Endpoints.h"
#include <string>
#include <vector>
#include <memory>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <signal.h>
#include <atomic>
#include <chrono>
#include <functional>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <random>

namespace PiTrac
{
enum class TaskStatus
{
    NotStarted,
    Starting,
    Running,
    Paused,
    Stopping,
    Stopped,
    Timeout,
    Failed,
    Crashed
};

class GSTaskBase
{
  public:

    GSTaskBase(const std::string &name)
        : name_(name)
        , task_id_(generateTaskId())
        , logger_(GSLogger::getInstance())
        , status_(TaskStatus::NotStarted)
        , should_stop_(false)
    {
        logInfo("Task created: " + name_ + " [" + task_id_ + "]");
        GSMessagerBase::createContext();
    }

    ~GSTaskBase()
    {
        if (isRunning())
        {
            end();
        }
        GSMessagerBase::destroyContext();
        logInfo("Task destroyed: " + name_);
    }

    bool run()
    {
        if (getStatus() == TaskStatus::Running)
        {
            logWarning("Task already running: " + name_);
            return false;
        }

        logInfo("Starting task: " + name_);
        changeStatus(TaskStatus::Starting);

        start_time_ = std::chrono::steady_clock::now();
        // Set process name
        prctl(PR_SET_NAME, name_.c_str(), 0, 0, 0);

        logInfo("Process started for task: " + name_);

        try {
            // Setup process environment
            if (!setupProcess())
            {
                logError("Failed to setup process");
                exit(1);
            }
            // Run main loop
            processMain();
        } catch (const std::exception &e) {
            logError("Exception in process main for task: " + name_ + " - " +
                     std::string(e.what()));
            exit(1);
        } catch (...) {
            logError("Unknown exception in process main for task: " + name_);
            exit(1);
        }

        // Cleanup
        cleanupProcess();

        logInfo("Process exiting for task: " + name_);
        exit(0);

        return true;
    }

    void end()
    {
        logInfo("Ending task: " + name_);

        if (!isRunning())
        {
            logWarning("Task not running, cannot end: " + name_);
            return;
        }
        preStopHook();

        changeStatus(TaskStatus::Stopping);

        should_stop_ = true;

        auto runtime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time_);

        logInfo("Task " + name_ + " execution completed. Runtime: " +
                std::to_string(runtime.count()) + "s, ");
    }

    void forceKill()
    {
        logInfo("Force killing task: " + name_);
        exit(1);
    }

    TaskStatus getStatus() const
    {
        return status_;
    }

    bool isRunning() const
    {
        return status_ == TaskStatus::Running;
    }

    const std::string &getTaskName() const
    {
        return name_;
    }

    const std::string &getTaskId() const
    {
        return task_id_;
    }

  protected:
    // @brief Name of the task for identification purposes / logging output
    std::string name_;
    // @brief Unique identifier for the task instance
    std::string task_id_;
    // @brief Current status of the task
    std::atomic<TaskStatus> status_;
    std::mutex status_mutex_;
    // @brief Atomic flag indicating whether the task should stop execution
    std::atomic<bool> should_stop_;
    // @brief Time point marking when the task started execution
    std::chrono::steady_clock::time_point start_time_;
    // @brief Logger instance for logging task-related messages
    std::shared_ptr<GSLogger> logger_;

    virtual bool setupProcess
    (
        void
    ) = 0;

    virtual void processMain
    (
        void
    ) = 0;

    virtual void cleanupProcess
    (
        void
    ) = 0;

    virtual void preStopHook
    (
        void
    )
    {
    }

    void logInfo(const std::string &message)
    {
        if (logger_)
        {
            logger_->info("[" + name_ + "] " + message);
        }
    }

    void logWarning(const std::string &message)
    {
        if (logger_)
        {
            logger_->warning("[" + name_ + "] " + message);
        }
    }

    void logError(const std::string &message)
    {
        if (logger_)
        {
            logger_->error("[" + name_ + "] " + message);
        }
    }

    void changeStatus(TaskStatus new_status)
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        if (status_ != new_status)
        {
            logInfo(name_ + " status changed: " +
                    taskStatusToString(status_) +
                    " -> " + taskStatusToString(new_status));
            status_ = new_status;
        }
    }

    std::string generateTaskId()
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(1000, 9999);

        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);

        std::stringstream ss;
        ss << name_ << "_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S")
           << "_" << dis(gen);

        return ss.str();
    }

    static std::string taskStatusToString
    (
        const TaskStatus &status
    )
    {
        switch (status)
        {
            case TaskStatus::NotStarted:
                return "NotStarted";
            case TaskStatus::Starting:
                return "Starting";
            case TaskStatus::Running:
                return "Running";
            case TaskStatus::Paused:
                return "Paused";
            case TaskStatus::Stopping:
                return "Stopping";
            case TaskStatus::Stopped:
                return "Stopped";
            case TaskStatus::Timeout:
                return "Timeout";
            case TaskStatus::Failed:
                return "Failed";
            case TaskStatus::Crashed:
                return "Crashed";
            default:
                return "Unknown";
        }
    }
};
} // namespace PiTrac

#endif // GSTASKBASE_H