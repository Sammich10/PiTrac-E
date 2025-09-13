#ifndef GSManager_H
#define GSManager_H

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
enum class ManagerStatus
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

enum class ManagerPriority
{
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

class GSManagerBase
{
  protected:
    std::string manager_name_;
    std::string manager_id_;
    ManagerStatus status_;
    ManagerPriority priority_;

    std::atomic<bool> should_stop_;
    std::atomic<bool> should_pause_;
    std::atomic<bool> is_running_;

    std::thread manager_thread_;

    // Timing and performance
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point end_time_;
    std::chrono::milliseconds timeout_duration_;

    // Statistics
    std::atomic<uint64_t> iterations_completed_;
    std::atomic<uint64_t> errors_count_;

    // Callbacks
    std::function<void(ManagerStatus)> status_change_callback_;
    std::function<void(const std::string &)> error_callback_;

    // Logger
    std::shared_ptr<GSLogger> logger_;

  public:
    GSManagerBase
    (
        const std::string &name,
        ManagerPriority priority = ManagerPriority::Normal
    );
    virtual ~GSManagerBase();

    // Core lifecycle methods (pure virtual)
    virtual bool setup() = 0;
    virtual bool initialize() = 0;
    virtual void execute() = 0;
    virtual void cleanup() = 0;

    // Manager control methods
    bool start();
    void stop();
    void pause();
    void resume();
    bool waitForCompletion
    (
        std::chrono::milliseconds timeout = std::chrono::milliseconds::max()
    );

    // Status and info methods
    ManagerStatus getStatus() const;
    ManagerPriority getPriority() const;
    void setPriority
    (
        ManagerPriority priority
    );

    const std::string &getManagerName() const
    {
        return manager_name_;
    }

    const std::string &getManagerId() const
    {
        return manager_id_;
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

    void setStatusChangeCallback(std::function<void(ManagerStatus)> callback)
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

    void logInfo
    (
        const std::string &message
    );
    void logWarning
    (
        const std::string &message
    );
    void logError
    (
        const std::string &message
    );

    // Manager state management for derived classes
    void setStatus
    (
        ManagerStatus status
    );
    void handlePause();
    bool checkTimeout();

    static std::string managerStatusToString(const ManagerStatus &status)
    {
        switch (status)
        {
            case ManagerStatus::NotStarted:
                return "NotStarted";
            case ManagerStatus::Initializing:
                return "Initializing";
            case ManagerStatus::Running:
                return "Running";
            case ManagerStatus::Paused:
                return "Paused";
            case ManagerStatus::Stopping:
                return "Stopping";
            case ManagerStatus::Completed:
                return "Completed";
            case ManagerStatus::Failed:
                return "Failed";
            case ManagerStatus::Timeout:
                return "Timeout";
            default:
                return "Unknown";
        }
    }

  private:
    void managerWrapper();
    void changeStatus
    (
        ManagerStatus new_status
    );
    std::string generateManagerId();
};
} // namespace PiTrac

#endif // GSManager_H