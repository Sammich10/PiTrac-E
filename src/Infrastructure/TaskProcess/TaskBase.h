#ifndef GSTASKBASE_H
#define GSTASKBASE_H

#include "Common/Utils/Logging/GSLogger.h"
#include "Common/System/System.h"
#include "Common/System/Endpoints.h"
#include "Infrastructure/TaskProcess/TaskStatus.h"
#include <string>
#include <vector>
#include <memory>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
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
class TaskBase
{
  public:

    /**
     * @brief Constructs a TaskBase object with the specified name. 
     * Creates a unique task ID, initializes the logger, and creates a ZMQ context for the process.
     *
     * @param name The name of the task for identification and logging purposes.
     */
    TaskBase(const std::string &name)
        : name_(name)
        , task_id_(generateTaskId())
        , logger_(GSLogger::getInstance())
        , status_(TaskStatus::NotStarted)
        , should_stop_(false)
    {
        logInfo("Task created: " + name_ + " [" + task_id_ + "]");
        MessagerBase::createContext();
    }
 
    /**
     * @brief Destructor for the TaskBase class.
     */
    ~TaskBase()
    {
        if (isRunning())
        {
            end();
        }
        MessagerBase::destroyContext();
        logInfo("Task destroyed: " + name_);
    }

    /**
     * @brief Primary task entry point, starts the task execution in the current process.
     * 
     * @throws std::exception if setupProcess or processMain throws an exception.
     * 
     * @return true upon successful task exit, false if the task failed to start.
     */
    bool run()
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

    void end()
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

    void forceKill()
    {
        logInfo("Force killing task: " + name_);
        exit(1);
    }

    /**
     * @brief Returns the current status of the task.
     * 
     * @return The current TaskStatus of the task.
     */
    TaskStatus getStatus() const
    {
        return status_;
    }

    /**
     * @brief Checks if the task is currently running.
     * 
     * @return true if the task is running, false otherwise.
     */
    bool isRunning() const
    {
        return status_ == TaskStatus::Running;
    }

    /**
     * @brief Returns the name of the task.
     * 
     * @return A constant reference to the task name string.
     */
    const std::string &getTaskName() const
    {
        return name_;
    }
    
    /**
     * @brief Returns the unique identifier for the task instance.
     * 
     * @return A constant reference to the task ID string.
     */
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

    /**
     * @brief Abstract method to set up the task process environment.
     * This method should be implemented by derived classes to perform any necessary setup before the task starts execution.
     */
    virtual bool setupProcess
    (
        void
    ) = 0;

    /**
     * @brief Abstract method that acts as the main execution loop for the task.
     * This method should be implemented by derived classes to define the primary behavior of the task.
     */
    virtual void processMain
    (
        void
    ) = 0;

    /**
     * @brief Abstract method to clean up the task process environment.
     * This method should be implemented by derived classes to perform any necessary cleanup after the task has
     */
    virtual void cleanupProcess
    (
        void
    ) = 0;

    /**
     * @brief Virtual method to end the task process. This method can be overridden by derived classes to implement custom behavior when ending the task.
     * After any overriding behavior, the derived class should call the base class implementation to ensure proper cleanup and logging.
     */
    virtual void endProcess
    (
        void
    )
    {
        logInfo("Ending task: " + name_);

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


    void logInfo(const std::string &message) const
    {
        if (logger_)
        {
            logger_->info(message);
        }
        else
        {
            printf("[INFO] %s\n", message.c_str());
        }
    }

    void logWarning(const std::string &message) const
    {
        if (logger_)
        {
            logger_->warning(message);
        }
        else
        {
            printf("[WARNING] %s\n", message.c_str());
        }
    }

    void logError(const std::string &message) const
    {
        if (logger_)
        {
            logger_->error(message);
        }
        else 
        {
            printf("[ERROR] %s\n", message.c_str());
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