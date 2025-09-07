#ifndef GSTASK_H
#define GSTASK_H

#include "Common/Utils/Logging/GSLogger.h"
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>

namespace PiTrac
{
enum class TaskStatus
{
    NotStarted,
    Initializing,
    Running,
    Paused,
    Stopping,
    Completed,
    Failed,
    Timeout
};

enum class TaskPriority
{
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

class GSTask
{
  protected:
    std::string task_name_;
    std::string task_id_;
    TaskStatus status_;
    TaskPriority priority_;

    std::atomic<bool> should_stop_;
    std::atomic<bool> should_pause_;
    std::atomic<bool> is_running_;

    std::thread task_thread_;
    mutable std::mutex status_mutex_;

    // Timing and performance
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point end_time_;
    std::chrono::milliseconds timeout_duration_;

    // Statistics
    std::atomic<uint64_t> iterations_completed_;
    std::atomic<uint64_t> errors_count_;

    // Callbacks
    std::function<void(TaskStatus)> status_change_callback_;
    std::function<void(const std::string &)> error_callback_;

    // Logger
    std::shared_ptr<GSLogger> logger_;

  public:
    GSTask(const std::string &name, TaskPriority priority = TaskPriority::Normal);
    virtual ~GSTask();

    // Core lifecycle methods (pure virtual)
    virtual bool initialize() = 0;
    virtual void execute() = 0;
    virtual void cleanup() = 0;

    // Task control methods
    bool start();
    void stop();
    void pause();
    void resume();
    bool waitForCompletion(std::chrono::milliseconds timeout = std::chrono::milliseconds::max());

    // Status and info methods
    TaskStatus getStatus() const;
    TaskPriority getPriority() const;
    void setPriority(TaskPriority priority);

    const std::string &getTaskName() const
    {
        return task_name_;
    }

    const std::string &getTaskId() const
    {
        return task_id_;
    }

    // Performance metrics
    std::chrono::duration<double> getRuntime() const;
    uint64_t getIterationsCompleted() const
    {
        return iterations_completed_.load();
    }

    uint64_t getErrorsCount() const
    {
        return errors_count_.load();
    }

    double getIterationsPerSecond() const;

    // Configuration
    void setTimeout(std::chrono::milliseconds timeout)
    {
        timeout_duration_ = timeout;
    }

    void setStatusChangeCallback(std::function<void(TaskStatus)> callback)
    {
        status_change_callback_ = callback;
    }

    void setErrorCallback(std::function<void(const std::string &)> callback)
    {
        error_callback_ = callback;
    }

    // Thread safety helpers
    bool isRunning() const
    {
        return is_running_.load();
    }

    bool shouldStop() const
    {
        return should_stop_.load();
    }

    bool shouldPause() const
    {
        return should_pause_.load();
    }

    // Utility methods for derived classes
    void incrementIterations()
    {
        iterations_completed_++;
    }

    void incrementErrors()
    {
        errors_count_++;
    }

    void logInfo(const std::string &message);
    void logWarning(const std::string &message);
    void logError(const std::string &message);

    // Task state management for derived classes
    void setStatus(TaskStatus status);
    void handlePause();
    bool checkTimeout();

  private:
    void taskWrapper();
    void changeStatus(TaskStatus new_status);
    std::string generateTaskId();
};
} // namespace PiTrac

#endif // GSTASK_H