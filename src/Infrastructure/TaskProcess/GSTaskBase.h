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
    GSTaskBase(const std::string &name);
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

    // Utility methods for derived classes
    void logInfo(const std::string &message);
    void logWarning(const std::string &message);
    void logError(const std::string &message);

  protected:
    // Internal methods available to derived classes
    void changeStatus(TaskStatus new_status);
    std::string generateTaskId();

    // Process entry point
    void processEntryPoint();

    // Hook methods that derived classes can override
    virtual bool preStartHook()
    {
        return true;
    }                                                 // Called before fork

    virtual void postStartHook()
    {
    }                                                 // Called after successful
                                                      // fork

    virtual void preStopHook()
    {
    }                                                 // Called before stop

    virtual void postStopHook()
    {
    }                                                 // Called after stop
};
} // namespace PiTrac

#endif // GSTASKBASE_H