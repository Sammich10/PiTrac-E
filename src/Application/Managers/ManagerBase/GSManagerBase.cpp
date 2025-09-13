#include "Application/Managers/ManagerBase/GSManagerBase.h"
#include <sstream>
#include <iomanip>
#include <random>

namespace PiTrac
{
GSManagerBase::GSManagerBase(const std::string &name, ManagerPriority priority)
    : manager_name_(name)
    , manager_id_(generateManagerId())
    , status_(ManagerStatus::NotStarted)
    , priority_(priority)
    , should_stop_(false)
    , should_pause_(false)
    , is_running_(false)
    , timeout_duration_(std::chrono::milliseconds::max())
    , iterations_completed_(0)
    , errors_count_(0)
    , logger_(GSLogger::getInstance())
{
    logInfo("Manager created: " + manager_name_ + " [" + manager_id_ + "]");
}

GSManagerBase::~GSManagerBase()
{
    if (manager_thread_.joinable())
    {
        stop();
        manager_thread_.join();
    }
    logInfo("Manager destroyed: " + manager_name_);
}

bool GSManagerBase::start()
{
    if (status_ == ManagerStatus::Running)
    {
        logWarning("Manager already running: " + manager_name_);
        return false;
    }

    if (status_ == ManagerStatus::Paused)
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
        // Start manager thread
        manager_thread_ = std::thread(&GSManagerBase::managerWrapper, this);
        logInfo("Manager started: " + manager_name_);
        return true;
    } catch (const std::exception &e) {
        logError("Failed to start manager: " + std::string(e.what()));
        changeStatus(ManagerStatus::Failed);
        return false;
    }
}

void GSManagerBase::stop()
{
    logInfo("Stopping manager: " + manager_name_);
    if (status_ == ManagerStatus::NotStarted || status_ == ManagerStatus::Completed ||
        status_ == ManagerStatus::Failed)
    {
        logInfo("Manager already in terminal state: " + manager_name_);
        return;
    }

    should_stop_.store(true);
    should_pause_.store(false);

    changeStatus(ManagerStatus::Stopping);

    if (manager_thread_.joinable())
    {
        logInfo("Joining " + manager_name_ + " execution thread...");
        if(!waitForCompletion(std::chrono::seconds(2)))
        {
            logWarning("Manager thread did not exit within timeout!");
        }
        logInfo(manager_name_ + " execution thread stopped");
    }

    logInfo("Manager stopped: " + manager_name_);
}

void GSManagerBase::pause()
{
    if (status_ == ManagerStatus::Running)
    {
        should_pause_ = true;
        changeStatus(ManagerStatus::Paused);
        logInfo("Manager paused: " + manager_name_);
    }
}

void GSManagerBase::resume()
{
    if (status_ == ManagerStatus::Paused)
    {
        should_pause_ = false;
        changeStatus(ManagerStatus::Running);
        logInfo("Manager resumed: " + manager_name_);
    }
}

bool GSManagerBase::waitForCompletion(std::chrono::milliseconds timeout)
{
    if (manager_thread_.joinable())
    {
        if (timeout == std::chrono::milliseconds::max())
        {
            manager_thread_.join();
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

ManagerStatus GSManagerBase::getStatus() const
{
    return status_;
}

ManagerPriority GSManagerBase::getPriority() const
{
    return priority_;
}

void GSManagerBase::setPriority(ManagerPriority priority)
{
    priority_ = priority;
    logInfo("Manager priority changed to: " + std::to_string(static_cast<int>(priority)));
}

std::chrono::duration<double> GSManagerBase::getRuntime() const
{
    auto end = (status_ == ManagerStatus::Running) ? std::chrono::steady_clock::now() : end_time_;
    return end - start_time_;
}

double GSManagerBase::getIterationsPerSecond() const
{
    auto runtime = getRuntime();
    if (runtime.count() > 0)
    {
        return static_cast<double>(iterations_completed_.load()) / runtime.count();
    }
    return 0.0;
}

void GSManagerBase::setStatus(ManagerStatus status)
{
    changeStatus(status);
}

void GSManagerBase::handlePause()
{
    while (should_pause_.load() && !should_stop_.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

bool GSManagerBase::checkTimeout()
{
    if (timeout_duration_ != std::chrono::milliseconds::max())
    {
        auto elapsed = std::chrono::steady_clock::now() - start_time_;
        if (elapsed > timeout_duration_)
        {
            logError("Manager timeout exceeded: " + manager_name_);
            changeStatus(ManagerStatus::Timeout);
            return true;
        }
    }
    return false;
}

void GSManagerBase::logInfo(const std::string &message)
{
    if (logger_)
    {
        logger_->info("[" + manager_name_ + "] " + message);
    }
}

void GSManagerBase::logWarning(const std::string &message)
{
    if (logger_)
    {
        logger_->warning("[" + manager_name_ + "] " + message);
    }
}

void GSManagerBase::logError(const std::string &message)
{
    if (logger_)
    {
        logger_->error("[" + manager_name_ + "] " + message);
    }

    if (error_callback_)
    {
        error_callback_(message);
    }
}

void GSManagerBase::managerWrapper()
{
    start_time_ = std::chrono::steady_clock::now();
    is_running_ = true;

    try {
        // Initialize
        changeStatus(ManagerStatus::Initializing);
        if (!initialize())
        {
            logError("Manager initialization failed");
            changeStatus(ManagerStatus::Failed);
            is_running_ = false;
            return;
        }

        // Execute main manager
        changeStatus(ManagerStatus::Running);
        execute();

        // Normal completion
        if (!should_stop_.load())
        {
            changeStatus(ManagerStatus::Completed);
        }
    } catch (const std::exception &e) {
        logError("Manager execution failed: " + std::string(e.what()));
        changeStatus(ManagerStatus::Failed);
    } catch (...) {
        logError("Manager execution failed with unknown exception");
        changeStatus(ManagerStatus::Failed);
    }

    // Cleanup
    try {
        cleanup();
    } catch (const std::exception &e) {
        logError("Manager cleanup failed: " + std::string(e.what()));
    }

    end_time_ = std::chrono::steady_clock::now();
    is_running_ = false;

    logInfo("Manager execution completed. Runtime: " +
            std::to_string(getRuntime().count()) + "s, " +
            "Iterations: " + std::to_string(iterations_completed_.load()) + ", " +
            "Errors: " + std::to_string(errors_count_.load()));
}

void GSManagerBase::changeStatus(ManagerStatus new_status)
{
    ManagerStatus old_status;
    old_status = status_;
    status_ = new_status;

    if (status_change_callback_)
    {
        status_change_callback_(new_status);
    }

    // Log status changes
    if (old_status != new_status)
    {
        logInfo("Manager status changed: " + managerStatusToString(old_status) +
                " -> " + managerStatusToString(new_status));
    }
}

std::string GSManagerBase::generateManagerId()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1000, 9999);

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << manager_name_ << "_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S")
       << "_" << dis(gen);

    return ss.str();
}
} // namespace PiTrac