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

    // @brief Name of the task for identification purposes / logging output
    std::string task_name_;

    // @brief Unique identifier for the task instance
    std::string task_id_;

    // @brief Current status of the task
    TaskStatus status_;

    // @brief Atomic flag indicating whether the task should stop execution
    std::atomic<bool> should_stop_;

    // @brief IPC endpoint for communication between the task and other
    // components
    std::string ipc_endpoint_;

    // @brief Time point marking when the task started execution
    std::chrono::steady_clock::time_point start_time_;
    // @brief Logger instance for logging task-related messages
    std::shared_ptr<GSLogger> logger_;

    // @brief Callback function to notify when the task status changes
    std::function<void(TaskStatus)> status_change_callback_;
    // @brief Callback function to notify when the process exits
    std::function<void(pid_t, int)> process_exit_callback_;

  public:
    /**
     * @brief Constructs a GSTaskBase object with the specified name.
     *
     * @param name The name to assign to the task.
     */
    GSTaskBase
    (
        const std::string &name
    );

    /**
     * @brief Virtual destructor for GSTaskBase.
     *
     * Ensures proper cleanup of derived classes when deleted through a base
     *class pointer.
     */
    virtual ~GSTaskBase();

    /**
     * @brief Sets up the process required for the task.
     *
     * This pure virtual function should be implemented by derived classes to
     *perform
     * any necessary initialization or configuration before the task process
     *starts.
     *
     * @return true if the setup was successful, false otherwise.
     */
    virtual bool setupProcess() = 0;

    /**
     * @brief Pure virtual function to execute the main processing logic of the
     *task.
     *
     * This method must be implemented by derived classes to define the core
     *behavior
     * of the task. It is called to perform the primary operations associated
     *with the task.
     */
    virtual void processMain() = 0;

    /**
     * @brief Cleans up resources and performs necessary finalization for the
     *process.
     *
     * This pure virtual function should be implemented by derived classes to
     *handle
     * any cleanup operations required when the process is finished or
     *terminated.
     */
    virtual void cleanupProcess() = 0;

    /**
     * @brief Starts the task process.
     *
     * This virtual function initiates the execution of the task.
     * Derived classes should override this method to implement specific start
     *logic.
     *
     * @return true if the task started successfully, false otherwise.
     */
    virtual bool start();

    /**
     * @brief Stops the execution of the task.
     *
     * This method should be overridden to implement the logic required to
     *safely
     * stop the task's processing. It may involve cleanup operations or resource
     * deallocation.
     */
    virtual void stop();

    /**
     * @brief Forcefully terminates the task, bypassing any graceful shutdown
     *procedures.
     *
     * This method should be used with caution, as it may leave resources in an
     *inconsistent state.
     * Implementations should ensure that all necessary cleanup is performed.
     */
    virtual void forceKill();

    /**
     * @brief Retrieves the current status of the task.
     *
     * @return TaskStatus The current status of the task.
     */
    TaskStatus getStatus() const;

    /**
     * @brief Retrieves the name of the task.
     *
     * @return A constant reference to the task name as a std::string.
     */
    const std::string &getTaskName() const
    {
        return task_name_;
    }

    /**
     * @brief Retrieves the unique identifier of the task.
     *
     * @return A constant reference to the task's ID string.
     */
    const std::string &getTaskId() const
    {
        return task_id_;
    }

    /**
     * @brief Checks if the task is currently running.
     *
     * @return true if the task is running, false otherwise.
     */
    virtual bool isRunning() const;

    /**
     * @brief Sets the IPC (Inter-Process Communication) endpoint.
     *
     * This function assigns the specified endpoint string to the internal
     * IPC endpoint variable, which is used for communication between processes.
     *
     * @param endpoint The IPC endpoint as a string.
     */
    void setIPCEndpoint(const std::string &endpoint)
    {
        ipc_endpoint_ = endpoint;
    }

    /**
     * @brief Sets the callback function to be invoked when the task status
     * changes.
     *
     * @param callback A std::function that takes a TaskStatus parameter and
     * returns void.
     */
    void setStatusChangeCallback(std::function<void(TaskStatus)> callback)
    {
        status_change_callback_ = callback;
    }

    /**
     * @brief Sets the callback function to be invoked when the process exits.
     *
     * This function allows you to specify a callback that will be called with
     * the
     * process ID and exit code when the associated process terminates.
     *
     * @param callback A std::function taking a pid_t (process ID) and an int
     *(exit code).
     */
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