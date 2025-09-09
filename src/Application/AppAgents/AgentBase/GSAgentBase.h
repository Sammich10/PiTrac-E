#ifndef GSAgent_H
#define GSAgent_H

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
enum class AgentStatus
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

enum class AgentPriority
{
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

class GSAgentBase
{
  protected:
    std::string agent_name_;
    std::string agent_id_;
    AgentStatus status_;
    AgentPriority priority_;

    std::atomic<bool> should_stop_;
    std::atomic<bool> should_pause_;
    std::atomic<bool> is_running_;

    std::thread agent_thread_;

    // Timing and performance
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point end_time_;
    std::chrono::milliseconds timeout_duration_;

    // Statistics
    std::atomic<uint64_t> iterations_completed_;
    std::atomic<uint64_t> errors_count_;

    // Callbacks
    std::function<void(AgentStatus)> status_change_callback_;
    std::function<void(const std::string &)> error_callback_;

    // Logger
    std::shared_ptr<GSLogger> logger_;

  public:
    GSAgentBase
    (
        const std::string &name,
        AgentPriority priority = AgentPriority::Normal
    );
    virtual ~GSAgentBase();

    // Core lifecycle methods (pure virtual)
    virtual bool setup() = 0;
    virtual bool initialize() = 0;
    virtual void execute() = 0;
    virtual void cleanup() = 0;

    // Agent control methods
    bool start();
    void stop();
    void pause();
    void resume();
    bool waitForCompletion
    (
        std::chrono::milliseconds timeout = std::chrono::milliseconds::max()
    );

    // Status and info methods
    AgentStatus getStatus() const;
    AgentPriority getPriority() const;
    void setPriority
    (
        AgentPriority priority
    );

    const std::string &getAgentName() const
    {
        return agent_name_;
    }

    const std::string &getAgentId() const
    {
        return agent_id_;
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

    void setStatusChangeCallback(std::function<void(AgentStatus)> callback)
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

    // Agent state management for derived classes
    void setStatus
    (
        AgentStatus status
    );
    void handlePause();
    bool checkTimeout();

    static std::string agentStatusToString(const AgentStatus &status)
    {
        switch (status)
        {
            case AgentStatus::NotStarted:
                return "NotStarted";
            case AgentStatus::Initializing:
                return "Initializing";
            case AgentStatus::Running:
                return "Running";
            case AgentStatus::Paused:
                return "Paused";
            case AgentStatus::Stopping:
                return "Stopping";
            case AgentStatus::Completed:
                return "Completed";
            case AgentStatus::Failed:
                return "Failed";
            case AgentStatus::Timeout:
                return "Timeout";
            default:
                return "Unknown";
        }
    }

  private:
    void agentWrapper();
    void changeStatus
    (
        AgentStatus new_status
    );
    std::string generateAgentId();
};
} // namespace PiTrac

#endif // GSAgent_H