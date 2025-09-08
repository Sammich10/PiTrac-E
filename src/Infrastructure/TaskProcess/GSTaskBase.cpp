#include "Infrastructure/TaskProcess/GSTaskBase.h"
#include "GSTaskBase.h"
#include <sys/prctl.h>
#include <errno.h>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <random>

namespace PiTrac
{
// Global pointer for signal handling
static GSTaskBase *g_current_task = nullptr;

GSTaskBase::GSTaskBase(const std::string &name)
    : task_name_(name)
    , task_id_(generateTaskId())
    , status_(TaskStatus::NotStarted)
    , child_pid_(-1)
    , should_stop_(false)
    , logger_(GSLogger::getInstance())
{
    logInfo("Task created: " + task_name_ + " [" + task_id_ + "]");
}

GSTaskBase::~GSTaskBase()
{
    if (isRunning())
    {
        stop();
        waitForExit(10);
    }
    logInfo("Task destroyed: " + task_name_);
}

bool GSTaskBase::start()
{
    std::lock_guard<std::mutex> lock(status_mutex_);

    if (status_ == TaskStatus::Running)
    {
        logWarning("Task already running: " + task_name_);
        return false;
    }

    logInfo("Starting task: " + task_name_);
    changeStatus(TaskStatus::Starting);

    // Pre-start hook, do any necessary setup before forking the child process (agents)
    if (!preStartHook())
    {
        logError("Pre-start hook failed");
        changeStatus(TaskStatus::Failed);
        return false;
    }

    // Fork process
    child_pid_ = fork();

    if (child_pid_ == -1)
    {
        // Fork failed
        logError("Failed to fork process: " + std::string(strerror(errno)));
        changeStatus(TaskStatus::Failed);
        return false;
    }
    else if (child_pid_ == 0)
    {
        // Child process
        childProcessEntryPoint();
        exit(0); // Child should never reach here
    }
    else
    {
        // Parent process
        start_time_ = std::chrono::steady_clock::now();
        changeStatus(TaskStatus::Running);
        logInfo("Task started with PID: " + std::to_string(child_pid_));

        postStartHook();
        return true;
    }
}

void GSTaskBase::stop()
{
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        if (!isRunning())
        {
            return;
        }

        preStopHook();
        changeStatus(TaskStatus::Stopping);
    }

    logInfo("Stopping task: " + task_name_);
    should_stop_ = true;

    // Send SIGTERM to child process
    if (child_pid_ > 0)
    {
        kill(child_pid_, SIGTERM);
    }

    postStopHook();
}

bool GSTaskBase::waitForExit(int timeout_seconds)
{
    logInfo("waitForExit called with timeout: " + std::to_string(timeout_seconds) + "s");
    
    if (child_pid_ <= 0)
    {
        logInfo("No child process to wait for");
        return true;
    }

    logInfo("Waiting for child process PID: " + std::to_string(child_pid_));
    
    auto start = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(timeout_seconds);

    while (std::chrono::steady_clock::now() - start < timeout)
    {
        int status;
        pid_t result = waitpid(child_pid_, &status, WNOHANG);

        logInfo("waitpid returned: " + std::to_string(result) + " for PID: " + std::to_string(child_pid_));

        if (result == child_pid_)
        {
            // Process exited
            int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            logInfo("Child process exited with code: " + std::to_string(exit_code));

            {
                std::lock_guard<std::mutex> lock(status_mutex_);
                if (exit_code == 0)
                {
                    changeStatus(TaskStatus::Stopped);
                    logInfo("Task exited normally");
                }
                else
                {
                    changeStatus(TaskStatus::Crashed);
                    logError("Task exited with code: " + std::to_string(exit_code));
                }
            }

            if (process_exit_callback_)
            {
                process_exit_callback_(child_pid_, exit_code);
            }

            child_pid_ = -1;
            return true;
        }
        else if (result == -1)
        {
            // Error or no child
            logError("waitpid failed: " + std::string(strerror(errno)));
            child_pid_ = -1;
            std::lock_guard<std::mutex> lock(status_mutex_);
            changeStatus(TaskStatus::Failed);
            return true;
        }
        else if (result == 0)
        {
            // Child still running, continue waiting
            logInfo("Child process still running, continuing to wait...");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Timeout reached
    logWarning("Timeout waiting for task to exit");
    return false;
}

void GSTaskBase::forceKill()
{
    if (child_pid_ > 0)
    {
        logWarning("Force killing task: " + task_name_);
        kill(child_pid_, SIGKILL);
        waitForExit(5);
    }
}

TaskStatus GSTaskBase::getStatus() const
{
    std::lock_guard<std::mutex> lock(status_mutex_);
    return status_;
}

bool GSTaskBase::isRunning() const
{
    std::lock_guard<std::mutex> lock(status_mutex_);
    return status_ == TaskStatus::Running && isChildProcessAlive();
}

void GSTaskBase::childProcessEntryPoint()
{
    // Set up signal handlers for child process
    g_current_task = this;
    setupSignalHandlers();

    // Set process name
    prctl(PR_SET_NAME, task_name_.c_str(), 0, 0, 0);

    task_name_ = "[CHILD] " + task_name_;

    logInfo("Child process started for task: " + task_name_);

    try {
        // Setup child process environment
        if (!setupChildProcess())
        {
            logError("Failed to setup child process");
            exit(1);
        }

        // Run main loop
        childProcessMain();
    } catch (const std::exception &e) {
        logError("Exception in child process: " + std::string(e.what()));
        exit(1);
    } catch (...) {
        logError("Unknown exception in child process");
        exit(1);
    }

    // Cleanup
    cleanupChildProcess();

    logInfo("Child process exiting for task: " + task_name_);
    exit(0);
}

bool GSTaskBase::isChildProcessAlive() const
{
    if (child_pid_ <= 0)
    {
        return false;
    }
    return true;
}

void GSTaskBase::setupSignalHandlers()
{
    signal(SIGTERM, signalHandler);
    signal(SIGINT, signalHandler);
    signal(SIGUSR1, signalHandler);
}

void GSTaskBase::signalHandler(int signal)
{
    if (g_current_task)
    {
        switch (signal)
        {
            case SIGTERM:
            case SIGINT:
                g_current_task->should_stop_ = true;
                g_current_task->logInfo("Received shutdown signal");
                g_current_task->stop();
                break;
            case SIGUSR1:
                g_current_task->logInfo("Received user signal 1");
                break;
        }
    }
}

void GSTaskBase::changeStatus(TaskStatus new_status)
{
    TaskStatus old_status = status_;
    status_ = new_status;

    if (status_change_callback_)
    {
        status_change_callback_(new_status);
    }

    if (old_status != new_status)
    {
        logInfo("Status changed: " + std::to_string(static_cast<int>(old_status)) +
                " -> " + std::to_string(static_cast<int>(new_status)));
    }
}

void GSTaskBase::logInfo(const std::string &message)
{
    if (logger_)
    {
        logger_->info("[" + task_name_ + "] " + message);
    }
}

void GSTaskBase::logWarning(const std::string &message)
{
    if (logger_)
    {
        logger_->warning("[" + task_name_ + "] " + message);
    }
}

void GSTaskBase::logError(const std::string &message)
{
    if (logger_)
    {
        logger_->error("[" + task_name_ + "] " + message);
    }
}

std::string GSTaskBase::generateTaskId()
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