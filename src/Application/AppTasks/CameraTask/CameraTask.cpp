#include "Application/AppTasks/CameraTask/CameraTask.h"
#include "Interfaces/Camera/imx296/imx296.h"

namespace PiTrac
{
CameraTask::CameraTask() : GSAgentTask("CameraTask")
{
    setRestartFailedAgents(true);
    setAgentCheckInterval(std::chrono::milliseconds(2000));
    camera_endpoints_[0] = "ipc:///tmp/camera_frames_1";
    camera_endpoints_[1] = "ipc:///tmp/camera_frames_2";
    camera_ids_[0] = "Camera_0";
    camera_ids_[1] = "Camera_1";
    logger_ = GSLogger::getInstance();
}

void CameraTask::configureAgents()
{
    logger_->info("Configuring Camera Agents...");
    std::unique_ptr<GSCameraInterface> camera_0 = std::make_unique<IMX296Camera>(0);
    // std::unique_ptr<GSCameraInterface> camera_2 =
    // std::make_unique<IMX296Camera>(1456, 1088, 2.8f,
    // PiTrac::TriggerMode::FREE_RUNNING);

    addAgent(CameraAgentFactory::createCameraAgent(std::move(camera_0), camera_endpoints_[0]));
}

bool CameraTask::preAgentStartHook()
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

void CameraTask::preStopHook()
{
    // Cleanup any lingering IPA processes manually for now... this should be
    // handled by the camera agent's closeCamera() method in the future.
    const int result = system("pkill -f raspberrypi_ipa 2>/dev/null");
}
} // namespace PiTrac