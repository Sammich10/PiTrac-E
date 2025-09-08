#include "Application/AppTasks/CameraAgentTask/CameraAgentTask.h"
#include "Interfaces/Camera/imx296/imx296.h"

namespace PiTrac
{
CameraAgentTask::CameraAgentTask() : GSAgentTask("CameraAgentTask")
{
    setRestartFailedAgents(true);
    setAgentCheckInterval(std::chrono::milliseconds(2000));
    setIPCEndpoint("ipc://camera_agent_task_endpoint");
}

bool CameraAgentTask::setupProcess()
{
    logger_ = GSLogger::getInstance();
    logInfo("CameraAgentTask process setup complete.");
    return true;
}

void CameraAgentTask::configureAgents()
{
    std::unique_ptr<GSCameraInterface> camera_1 = std::make_unique<IMX296Camera>(1456,
                                                                                 1088,
                                                                                 2.8f,
                                                                                 PiTrac::TriggerMode::FREE_RUNNING);
    // std::unique_ptr<GSCameraInterface> camera_2 =
    // std::make_unique<IMX296Camera>(1456, 1088, 2.8f,
    // PiTrac::TriggerMode::FREE_RUNNING);

    addAgent(CameraAgentFactory::createCameraAgent(std::move(camera_1), "1", ipc_endpoint_));
}

bool CameraAgentTask::preAgentStartHook()
{
    // Configure agents before starting
    try {
        configureAgents();
        logInfo("Configured " + std::to_string(agents_.size()) + " agents");
        return true;
    } catch (const std::exception &e) {
        logError("Failed to configure agents: " + std::string(e.what()));
        return false;
    }
    return true; // Pre-agent start hook implementation
}

void CameraAgentTask::postAgentStartHook()
{
    // Post-agent start hook implementation
}

void CameraAgentTask::onAgentFailedHook(std::shared_ptr<GSAgentBase> agent)
{
    // Agent failed hook implementation
}

void CameraAgentTask::agentMonitoringLoopHook()
{
    // Agent monitoring loop hook implementation
}

void CameraAgentTask::preStopHook()
{
    // Pre-stop hook implementation, clean up resources before stopping the task
}
} // namespace PiTrac