#ifndef GSTASKBASE_H
#define GSTASKBASE_H

#include "Common/Utils/Logging/GSLogger.h"
#include <string>
#include <vector>
#include <memory>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include <mutex>

namespace PiTrac
{
enum class TaskStatus
{
    NotStarted,
    Starting,
    Running,
    Stopping,
    Stopped,
    Failed,
    Crashed
};

class GSTaskBase
{
  protected:
    std::string task_name_;
    std::string task_id_;
    TaskStatus status_;

    std::atomic<bool> should_stop_;

    // IPC configuration
    std::string ipc_endpoint_;

    // Monitoring
    std::chrono::steady_clock::time_point start_time_;
    std::shared_ptr<GSLogger> logger_;

    // Callbacks
    std::function<void(TaskStatus)> status_change_callback_;
    std::function<void(pid_t, int)> process_exit_callback_;

  public:
    GSTaskBase
    (
        const std::string &name
    );
    virtual ~GSTaskBase();

    // Pure virtual methods for task-specific behavior
    virtual bool setupProcess() = 0;      // Called in child process before
    // task start
    virtual void processMain() = 0;       // Main loop for child process
    virtual void cleanupProcess() = 0;    // Called in child process on
    // shutdown

    // Task lifecycle (can be overridden but has default implementation)
    virtual bool start();
    virtual void stop();
    virtual void forceKill();

    // Status and monitoring
    TaskStatus getStatus() const;

    const std::string &getTaskName() const
    {
        return task_name_;
    }

    const std::string &getTaskId() const
    {
        return task_id_;
    }

    virtual bool isRunning() const;

    // Configuration
    void setIPCEndpoint(const std::string &endpoint)
    {
        ipc_endpoint_ = endpoint;
    }

    void setStatusChangeCallback(std::function<void(TaskStatus)> callback)
    {
        status_change_callback_ = callback;
    }

    void setProcessExitCallback(std::function<void(pid_t, int)> callback)
    {
        process_exit_callback_ = callback;
    }

  protected:

    /**
     * @brief Logs an informational message.
     *
     * This function records the provided message as an informational log entry.
     *
     * @param message The message to be logged.
     */
    void logInfo
    (
        const std::string &message
    );

    /**
     * @brief Logs a warning message.
     *
     * This function records a warning message, typically used to indicate
     * non-critical issues or unexpected behavior that does not prevent
     * program execution.
     *
     * @param message The warning message to be logged.
     */
    void logWarning
    (
        const std::string &message
    );

    /**
     * @brief Logs an error message.
     *
     * This function records the provided error message for diagnostic or
     * debugging purposes.
     *
     * @param message The error message to be logged.
     */
    void logError
    (
        const std::string &message
    );

    /**
     * @brief Changes the status of the task to the specified new status.
     *
     * This method updates the current status of the task. It may trigger
     * additional actions or notifications depending on the implementation.
     *
     * @param new_status The new status to set for the task.
     */
    void changeStatus
    (
        TaskStatus new_status
    );

    /**
     * @brief Generates a unique identifier for a task.
     *
     * @return std::string A unique task identifier.
     */
    std::string generateTaskId
    (
        void
    );

    /**
     * @brief Entry point for processing tasks.
     *
     * This method serves as the main entry point for executing the task's
     * processing logic.
     * Override this function in derived classes to implement specific task
     * behavior.
     */
    void processEntryPoint
    (
        void
    );

    /**
     * @brief Hook method called before the process is forked.
     *
     * This virtual function can be overridden to perform any setup or checks
     * required before the process starts. Returning false will prevent the
     * process
     * from starting.
     *
     * @return true if the process can proceed to start; false otherwise.
     */
    virtual bool preStartHook
    (
        void
    )
    {
        return true;
    }

    /**
     * @brief Hook method called before the task is stopped.
     *
     * This virtual function can be overridden by derived classes to implement
     * custom behavior that should occur immediately before the task stops.
     * The default implementation does nothing and returns true.
     *
     * @return true if the pre-stop actions were successful; false otherwise.
     */
    virtual void preStopHook
    (
        void
    )
    {
<<<<<<< HEAD
        return true;
=======
>>>>>>> develop
    }

    /**
     * @brief Hook method called after the task has started.
     *
     * This virtual function can be overridden by derived classes to implement
     * custom behavior that should occur immediately after the task starts.
     * The default implementation does nothing.
     */
    virtual void postStopHook
    (
        void
    )
    {
    }

    /**
     * @brief Converts a TaskStatus enum value to its corresponding string
     * representation.
     *
     * @param status The TaskStatus value to convert.
     * @return A string representing the given TaskStatus value. Returns
     *"Unknown" if the status is not recognized.
     */
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
            case TaskStatus::Stopping:
                return "Stopping";
            case TaskStatus::Stopped:
                return "Stopped";
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