#include "Application/AppAgents/AgentBase/GSAgentBase.h"
#include <sstream>
#include <iomanip>
#include <random>

namespace PiTrac
{
GSAgentBase::GSAgentBase(const std::string &name, AgentPriority priority)
    : agent_name_(name)
    , agent_id_(generateAgentId())
    , status_(AgentStatus::NotStarted)
    , priority_(priority)
    , should_stop_(false)
    , should_pause_(false)
    , is_running_(false)
    , timeout_duration_(std::chrono::milliseconds::max())
    , iterations_completed_(0)
    , errors_count_(0)
    , logger_(std::make_shared<GSLogger>(logger_level::info))
{
    logInfo("Agent created: " + agent_name_ + " [" + agent_id_ + "]");
}

GSAgentBase::~GSAgentBase()
{
    if (agent_thread_.joinable())
    {
        stop();
        agent_thread_.join();
    }
    logInfo("Agent destroyed: " + agent_name_);
}

bool GSAgentBase::start()
{
    std::lock_guard<std::mutex> lock(status_mutex_);

    if (status_ == AgentStatus::Running)
    {
        logWarning("Agent already running: " + agent_name_);
        return false;
    }

    if (status_ == AgentStatus::Paused)
    {
        resume();
        return true;
    }

    // Reset state
    should_stop_ = false;
    should_pause_ = false;
    iterations_completed_ = 0;
    errors_count_ = 0;

    try {
        // Start agent thread
        agent_thread_ = std::thread(&GSAgentBase::agentWrapper, this);
        logInfo("Agent started: " + agent_name_);
        return true;
    } catch (const std::exception &e) {
        logError("Failed to start agent: " + std::string(e.what()));
        changeStatus(AgentStatus::Failed);
        return false;
    }
}

void GSAgentBase::stop()
{
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        if (status_ == AgentStatus::NotStarted || status_ == AgentStatus::Completed ||
            status_ == AgentStatus::Failed)
        {
            return;
        }

        changeStatus(AgentStatus::Stopping);
    }

    should_stop_ = true;
    should_pause_ = false;

    if (agent_thread_.joinable())
    {
        agent_thread_.join();
    }

    logInfo("Agent stopped: " + agent_name_);
}

void GSAgentBase::pause()
{
    std::lock_guard<std::mutex> lock(status_mutex_);
    if (status_ == AgentStatus::Running)
    {
        should_pause_ = true;
        changeStatus(AgentStatus::Paused);
        logInfo("Agent paused: " + agent_name_);
    }
}

void GSAgentBase::resume()
{
    std::lock_guard<std::mutex> lock(status_mutex_);
    if (status_ == AgentStatus::Paused)
    {
        should_pause_ = false;
        changeStatus(AgentStatus::Running);
        logInfo("Agent resumed: " + agent_name_);
    }
}

bool GSAgentBase::waitForCompletion(std::chrono::milliseconds timeout)
{
    if (agent_thread_.joinable())
    {
        if (timeout == std::chrono::milliseconds::max())
        {
            agent_thread_.join();
            return true;
        }
        else
        {
            // Timed wait implementation
            auto start = std::chrono::steady_clock::now();
            while (isRunning() && (std::chrono::steady_clock::now() - start) < timeout)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            return !isRunning();
        }
    }
    return true;
}

AgentStatus GSAgentBase::getStatus() const
{
    std::lock_guard<std::mutex> lock(status_mutex_);
    return status_;
}

AgentPriority GSAgentBase::getPriority() const
{
    return priority_;
}

void GSAgentBase::setPriority(AgentPriority priority)
{
    priority_ = priority;
    logInfo("Agent priority changed to: " + std::to_string(static_cast<int>(priority)));
}

std::chrono::duration<double> GSAgentBase::getRuntime() const
{
    auto end = (status_ == AgentStatus::Running) ? std::chrono::steady_clock::now() : end_time_;
    return end - start_time_;
}

double GSAgentBase::getIterationsPerSecond() const
{
    auto runtime = getRuntime();
    if (runtime.count() > 0)
    {
        return static_cast<double>(iterations_completed_.load()) / runtime.count();
    }
    return 0.0;
}

void GSAgentBase::setStatus(AgentStatus status)
{
    changeStatus(status);
}

void GSAgentBase::handlePause()
{
    while (should_pause_.load() && !should_stop_.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

bool GSAgentBase::checkTimeout()
{
    if (timeout_duration_ != std::chrono::milliseconds::max())
    {
        auto elapsed = std::chrono::steady_clock::now() - start_time_;
        if (elapsed > timeout_duration_)
        {
            logError("Agent timeout exceeded: " + agent_name_);
            changeStatus(AgentStatus::Timeout);
            return true;
        }
    }
    return false;
}

void GSAgentBase::logInfo(const std::string &message)
{
    if (logger_)
    {
        logger_->info("[" + agent_name_ + "] " + message);
    }
}

void GSAgentBase::logWarning(const std::string &message)
{
    if (logger_)
    {
        logger_->warning("[" + agent_name_ + "] " + message);
    }
}

void GSAgentBase::logError(const std::string &message)
{
    if (logger_)
    {
        logger_->error("[" + agent_name_ + "] " + message);
    }

    if (error_callback_)
    {
        error_callback_(message);
    }
}

void GSAgentBase::agentWrapper()
{
    start_time_ = std::chrono::steady_clock::now();
    is_running_ = true;

    try {
        // Initialize
        changeStatus(AgentStatus::Initializing);
        if (!initialize())
        {
            logError("Agent initialization failed");
            changeStatus(AgentStatus::Failed);
            is_running_ = false;
            return;
        }

        // Execute main agent 
        changeStatus(AgentStatus::Running);
        execute();

        // Normal completion
        if (!should_stop_.load())
        {
            changeStatus(AgentStatus::Completed);
        }
    } catch (const std::exception &e) {
        logError("Agent execution failed: " + std::string(e.what()));
        changeStatus(AgentStatus::Failed);
    } catch (...) {
        logError("Agent execution failed with unknown exception");
        changeStatus(AgentStatus::Failed);
    }

    // Cleanup
    try {
        cleanup();
    } catch (const std::exception &e) {
        logError("Agent cleanup failed: " + std::string(e.what()));
    }

    end_time_ = std::chrono::steady_clock::now();
    is_running_ = false;

    logInfo("Agent execution completed. Runtime: " +
            std::to_string(getRuntime().count()) + "s, " +
            "Iterations: " + std::to_string(iterations_completed_.load()) + ", " +
            "Errors: " + std::to_string(errors_count_.load()));
}

void GSAgentBase::changeStatus(AgentStatus new_status)
{
    AgentStatus old_status;
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        old_status = status_;
        status_ = new_status;
    }

    if (status_change_callback_)
    {
        status_change_callback_(new_status);
    }

    // Log status changes
    if (old_status != new_status)
    {
        logInfo("Status changed: " + std::to_string(static_cast<int>(old_status)) +
                " -> " + std::to_string(static_cast<int>(new_status)));
    }
}

std::string GSAgentBase::generateAgentId()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1000, 9999);

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << agent_name_ << "_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S")
       << "_" << dis(gen);

    return ss.str();
}
} // namespace PiTrac