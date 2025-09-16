#include "Application/Tasks/SystemTask/SystemTask.h"

namespace PiTrac
{

SystemTask::SystemTask()
    : GSManagerTask("SystemTask")
{
    setRestartFailedManagers(true);
    setManagerCheckInterval(std::chrono::milliseconds(2000));
}

bool SystemTask::setupProcess()
{
    return true;
}

void SystemTask::configureManagers()
{
    logger_->info("Configuring System Task Managers...");
    // Create and add system manager
    addManager(std::make_shared<SystemManager>());

    logger_->info("Configured system task with " + std::to_string(managers_.size()) + " managers.");
}

bool SystemTask::preManagerStartHook()
{
    try {
        configureManagers();
        logInfo("Configured " + std::to_string(managers_.size()) + " managers");
        return true;
    } catch (const std::exception &e) {
        logError("Failed to configure managers: " + std::string(e.what()));
        return false;
    }
    return true; 
}

void SystemTask::cleanupProcess()
{
    logInfo("Cleaning up System Task...");
    // Perform any necessary cleanup here
    logInfo("System Task cleanup complete.");
}

} // namespace PiTrac