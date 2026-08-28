#ifndef GSTASKBASE_H
#define GSTASKBASE_H

#include "Common/Utils/Logging/GSLogger.h"
#include "Common/System/System.h"
#include "Common/System/Endpoints.h"
#include "Foundation/TaskProcess/TaskStatus.h"
#include "Infrastructure/Messaging/Messagers/MessagerBase.h"
#include "Infrastructure/Messaging/MessageInterface.h"
#include <string>
#include <thread>
#include <list>
#include <array>
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
    TaskBase
    (
        const std::string &name
    );

    /**
     * @brief Destructor for the TaskBase class.
     */
    ~TaskBase();

    /**
     * @brief Primary task entry point, starts the task execution in the current process.
     *
     * @throws std::exception if setupProcess or processMain throws an exception.
     *
     * @return true upon successful task exit, false if the task failed to start.
     */
    bool run();

    /**
     * @brief Signals the task to end its execution gracefully.
     * If the task is not running, a warning is logged.
     * Changes the task status to Stopping and sets the should_stop_ flag.
     * Logs the total runtime of the task upon completion.
     */
    void end();

    /**
     * @brief Forces the task to terminate immediately.
     * Logs the force kill action and exits the process with a failure status.
     */
    void forceKill();

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

    /**
     * @brief Nested class to handle event-driven actions in a separate thread.
     * This class encapsulates the logic for running a specified action in a separate thread,
     * while also managing a messager and a message handler for processing messages.
     * The action is executed in the run() method, which is called when the thread starts.
     * The thread is joined in the destructor to ensure proper cleanup.
     * The EventThread class is intended to be used by derived classes of TaskBase to handle
     * specific event-driven actions related to the task's operation driven through the ZMQ middleware layer.
     */
    class EventThread
    {
      public:
        EventThread(std::shared_ptr<MessagerBase> messager, std::function<void(const std::unique_ptr<MessageInterface> &)> message_handler)
            : messager_(messager)
            , message_handler_(message_handler)
            , logger_(GSLogger::getInstance())
        {
        }

        ~EventThread()
        {
            if (thread_.joinable())
            {
                thread_.join();
            }
        }

        enum class ThreadStatus
        {
            NotStarted,
            Running,
            RequestStop,
            Stopped,
            Exited
        };

        enum class ExitStatus
        {
            Success,
            Failure
        };

        ExitStatus getExitStatus() const
        {
            return exit_status_;
        }

        ThreadStatus getStatus() const
        {
            return status_;
        }

        void bindEndpoint(const std::string &endpoint)
        {
            if (messager_)
            {
                messager_->bind(endpoint);
            }
        }

        void connectEndpoint(const std::string &endpoint)
        {
            if (messager_)
            {
                messager_->connect(endpoint);
            }
        }

        void start()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (status_ == ThreadStatus::NotStarted)
            {
                status_ = ThreadStatus::Running;
                thread_ = std::thread(&EventThread::waitForEvent, this);
            }
        }

        void pause()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (status_ == ThreadStatus::Running)
            {
                status_ = ThreadStatus::Stopped;
            }
        }

        void stop()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (status_ == ThreadStatus::Running)
            {
                status_ = ThreadStatus::Stopped;
            }
        }

      private:
        void waitForEvent()
        {
            std::unique_ptr<MessageInterface> message;
            MessagerBase::RequestStatus mstatus;
            do
            {
                mstatus = messager_->pollMessage(message);
                if(status_ >= ThreadStatus::RequestStop)
                {
                    break;
                }
                if (mstatus == MessagerBase::RequestStatus::Success && message)
                {
                    message_handler_(message);
                }
            }
            while(1);
        }

        std::mutex mutex_;
        // std::function<void(std::shared_ptr<MessagerBase>)> action_;
        std::shared_ptr<MessagerBase> messager_;
        std::function<void(const std::unique_ptr<MessageInterface> &)> message_handler_;
        std::thread thread_;
        std::atomic<ThreadStatus> status_ = ThreadStatus::NotStarted;
        std::atomic<ExitStatus> exit_status_ = ExitStatus::Success;
        std::shared_ptr<GSLogger> logger_;
    };

    // @brief List of event threads managed by the task, each handling specific event-driven actions.
    std::list<std::unique_ptr<EventThread> > event_threads_;
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