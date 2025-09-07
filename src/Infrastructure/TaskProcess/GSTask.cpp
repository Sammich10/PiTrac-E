#include "GSTask.h"
#include <sstream>
#include <iomanip>
#include <random>

namespace PiTrac
{
GSTask::GSTask(const std::string &name, TaskPriority priority)
    : task_name_(name)
    , task_id_(generateTaskId())
    , status_(TaskStatus::NotStarted)
    , priority_(priority)
    , should_stop_(false)
    , should_pause_(false)
    , is_running_(false)
    , timeout_duration_(std::chrono::milliseconds::max())
    , iterations_completed_(0)
    , errors_count_(0)
    , logger_(std::make_shared<GSLogger>(logger_level::info))
{
    logInfo("Task created: " + task_name_ + " [" + task_id_ + "]");
}

GSTask::~GSTask()
{
    if (task_thread_.joinable())
    {
        stop();
        task_thread_.join();
    }
    logInfo("Task destroyed: " + task_name_);
}

bool GSTask::start()
{
    std::lock_guard<std::mutex> lock(status_mutex_);

    if (status_ == TaskStatus::Running)
    {
        logWarning("Task already running: " + task_name_);
        return false;
    }

    if (status_ == TaskStatus::Paused)
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
        // Start task thread
        task_thread_ = std::thread(&GSTask::taskWrapper, this);
        logInfo("Task started: " + task_name_);
        return true;
    } catch (const std::exception &e) {
        logError("Failed to start task: " + std::string(e.what()));
        changeStatus(TaskStatus::Failed);
        return false;
    }
}

void GSTask::stop()
{
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        if (status_ == TaskStatus::NotStarted || status_ == TaskStatus::Completed ||
            status_ == TaskStatus::Failed)
        {
            return;
        }

        changeStatus(TaskStatus::Stopping);
    }

    should_stop_ = true;
    should_pause_ = false;

    if (task_thread_.joinable())
    {
        task_thread_.join();
    }

    logInfo("Task stopped: " + task_name_);
}

void GSTask::pause()
{
    std::lock_guard<std::mutex> lock(status_mutex_);
    if (status_ == TaskStatus::Running)
    {
        should_pause_ = true;
        changeStatus(TaskStatus::Paused);
        logInfo("Task paused: " + task_name_);
    }
}

void GSTask::resume()
{
    std::lock_guard<std::mutex> lock(status_mutex_);
    if (status_ == TaskStatus::Paused)
    {
        should_pause_ = false;
        changeStatus(TaskStatus::Running);
        logInfo("Task resumed: " + task_name_);
    }
}

bool GSTask::waitForCompletion(std::chrono::milliseconds timeout)
{
    if (task_thread_.joinable())
    {
        if (timeout == std::chrono::milliseconds::max())
        {
            task_thread_.join();
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

TaskStatus GSTask::getStatus() const
{
    std::lock_guard<std::mutex> lock(status_mutex_);
    return status_;
}

TaskPriority GSTask::getPriority() const
{
    return priority_;
}

void GSTask::setPriority(TaskPriority priority)
{
    priority_ = priority;
    logInfo("Task priority changed to: " + std::to_string(static_cast<int>(priority)));
}

std::chrono::duration<double> GSTask::getRuntime() const
{
    auto end = (status_ == TaskStatus::Running) ? std::chrono::steady_clock::now() : end_time_;
    return end - start_time_;
}

double GSTask::getIterationsPerSecond() const
{
    auto runtime = getRuntime();
    if (runtime.count() > 0)
    {
        return static_cast<double>(iterations_completed_.load()) / runtime.count();
    }
    return 0.0;
}

void GSTask::setStatus(TaskStatus status)
{
    changeStatus(status);
}

void GSTask::handlePause()
{
    while (should_pause_.load() && !should_stop_.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

bool GSTask::checkTimeout()
{
    if (timeout_duration_ != std::chrono::milliseconds::max())
    {
        auto elapsed = std::chrono::steady_clock::now() - start_time_;
        if (elapsed > timeout_duration_)
        {
            logError("Task timeout exceeded: " + task_name_);
            changeStatus(TaskStatus::Timeout);
            return true;
        }
    }
    return false;
}

void GSTask::logInfo(const std::string &message)
{
    if (logger_)
    {
        logger_->info("[" + task_name_ + "] " + message);
    }
}

void GSTask::logWarning(const std::string &message)
{
    if (logger_)
    {
        logger_->warning("[" + task_name_ + "] " + message);
    }
}

void GSTask::logError(const std::string &message)
{
    if (logger_)
    {
        logger_->error("[" + task_name_ + "] " + message);
    }

    if (error_callback_)
    {
        error_callback_(message);
    }
}

void GSTask::taskWrapper()
{
    start_time_ = std::chrono::steady_clock::now();
    is_running_ = true;

    try {
        // Initialize
        changeStatus(TaskStatus::Initializing);
        if (!initialize())
        {
            logError("Task initialization failed");
            changeStatus(TaskStatus::Failed);
            is_running_ = false;
            return;
        }

        // Execute main task
        changeStatus(TaskStatus::Running);
        execute();

        // Normal completion
        if (!should_stop_.load())
        {
            changeStatus(TaskStatus::Completed);
        }
    } catch (const std::exception &e) {
        logError("Task execution failed: " + std::string(e.what()));
        changeStatus(TaskStatus::Failed);
    } catch (...) {
        logError("Task execution failed with unknown exception");
        changeStatus(TaskStatus::Failed);
    }

    // Cleanup
    try {
        cleanup();
    } catch (const std::exception &e) {
        logError("Task cleanup failed: " + std::string(e.what()));
    }

    end_time_ = std::chrono::steady_clock::now();
    is_running_ = false;

    logInfo("Task execution completed. Runtime: " +
            std::to_string(getRuntime().count()) + "s, " +
            "Iterations: " + std::to_string(iterations_completed_.load()) + ", " +
            "Errors: " + std::to_string(errors_count_.load()));
}

void GSTask::changeStatus(TaskStatus new_status)
{
    TaskStatus old_status;
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

std::string GSTask::generateTaskId()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1000, 9999);

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << task_name_ << "_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S")
       << "_" << dis(gen);

    return ss.str();
}
} // namespace PiTrac