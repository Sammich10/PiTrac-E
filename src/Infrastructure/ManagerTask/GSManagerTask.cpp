#include "Infrastructure/ManagerTask/GSManagerTask.h"

namespace PiTrac
{
GSManagerTask::GSManagerTask(const std::string &name)
    : GSTaskBase(name)
    , restart_failed_managers_(false)
    , manager_check_interval_(std::chrono::milliseconds(1000))
    , manager_task_ipc_endpoint_("ipc://manager_task")
    , manager_task_ipc_subscriber_(std::make_unique<GSMessagerBase>(GSMessagerBase::SocketType::Subscriber))
{
    logInfo("Manager task created: " + task_name_ + " [" + task_id_ + "]");
}

GSManagerTask::~GSManagerTask()
{
    manager_task_ipc_subscriber_->stop();
}

void GSManagerTask::processMain()
{
    logInfo("Starting manager task main loop");

    // Pre-manager start hook
    if (!preManagerStartHook())
    {
        logError("Pre-manager start hook failed");
        return;
    }

    if(!setupAllManagers())
    {
        logError("Failed to setup all managers");
        return;
    }

    // Start all managers
    if (!startAllManagers())
    {
        logError("Failed to start managers");
        return;
    }

    postManagerStartHook();

    changeStatus(TaskStatus::Running);

    // Main monitoring loop
    while (!should_stop_)
    {
        // Check manager health
        if (!areAllManagersRunning())
        {
            if (restart_failed_managers_)
            {
                restartFailedManagers();
            }
            else
            {
                logError("Some managers failed and restart is disabled");
                break;
            }
        }

        // Custom hook for derived classes
        managerMonitoringLoopHook();

        std::this_thread::sleep_for(manager_check_interval_);
    }

    logInfo("Manager task main loop ended. Stopping all managers.");
    stopAllManagers();
}

bool GSManagerTask::setupAllManagers()
{
    logInfo("Setting up all managers");

    for (auto &manager : managers_)
    {
        if (!manager->setup())
        {
            logError("Failed to setup manager: " + manager->getManagerName());
            return false;
        }
    }

    logInfo("All managers set up successfully");
    return true;
}

bool GSManagerTask::startAllManagers()
{
    logInfo("Starting " + std::to_string(managers_.size()) + " managers");

    for (auto &manager : managers_)
    {
        if (!manager->start())
        {
            logError("Failed to start manager: " + manager->getManagerName());
            return false;
        }
    }

    logInfo("All managers started successfully");
    return true;
}

void GSManagerTask::stopAllManagers()
{
    logInfo("Stopping all managers");

    for (auto &manager : managers_)
    {
        logInfo("Stopping manager: " + manager->getManagerName());
        manager->stop();
        if(!manager->waitForCompletion(std::chrono::seconds(5)))
        {
            logWarning("Manager did not stop in time: " + manager->getManagerName());
        }
    }

    logInfo("All managers stopped");
}

bool GSManagerTask::areAllManagersRunning()
{
    for (const auto &manager : managers_)
    {
        if (!manager->isRunning())
        {
            return false;
        }
    }
    return true;
}

void GSManagerTask::restartFailedManagers()
{
    for (auto &manager : managers_)
    {
        if (!manager->isRunning())
        {
            logWarning("Restarting failed manager: " + manager->getManagerName());
            onManagerFailedHook(manager);

            if (!manager->start())
            {
                logError("Failed to restart manager: " + manager->getManagerName());
            }
        }
    }
}

void GSManagerTask::addManager(std::shared_ptr<GSManagerBase> manager)
{
    managers_.push_back(manager);
    logInfo("Added manager: " + manager->getManagerName() + " to task: " + task_name_);
}

void GSManagerTask::removeManager(const std::string &manager_id)
{
    auto it = std::remove_if(managers_.begin(), managers_.end(),
                             [&manager_id](const std::shared_ptr<GSManagerBase> &manager) {
            return manager->getManagerId() == manager_id;
        });

    if (it != managers_.end())
    {
        managers_.erase(it, managers_.end());
        logInfo("Removed manager: " + manager_id + " from task: " + task_name_);
    }
}

std::vector<std::shared_ptr<GSManagerBase> > GSManagerTask::getManagers() const
{
    return managers_;
}
} // namespace PiTrac