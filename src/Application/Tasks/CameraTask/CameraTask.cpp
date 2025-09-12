#include "Application/Tasks/CameraTask/CameraTask.h"
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

bool CameraTask::setupProcess()
{
    // Configure agents before starting
    cameraManager_ = std::make_shared<libcamera::CameraManager>();
    // Initialize camera manager
    int ret = cameraManager_->start();
    if (ret)
    {
        logger_->error("Failed to start camera manager");
        return false;
    }
    agent_task_ipc_subscriber_->bind(agent_task_ipc_endpoint_);
    logInfo("Agent task IPC subscriber bound to: " + agent_task_ipc_endpoint_);
    return true;
}

void CameraTask::configureAgents()
{
    logger_->info("Configuring Camera Task Agents...");
    const size_t num_cameras = 2; // Update this to the actual number of cameras
                                  // available

    for(size_t i = 0; i < num_cameras; i++)
    {
        std::unique_ptr<GSCameraInterface> camera = std::make_unique<IMX296Camera>(i, cameraManager_);
        std::shared_ptr<FrameBuffer> frame_buffer = std::make_shared<FrameBuffer>(128);

        addAgent(CameraAgentFactory::createCameraAgent(std::move(camera), frame_buffer, i));
        addAgent(FrameProcessorAgentFactory::create(std::move(frame_buffer), i));
    }
    logger_->info("Configured " + std::to_string(num_cameras) + " camera task agents.");
}

bool CameraTask::preAgentStartHook()
{
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

void CameraTask::cleanupProcess()
{
    logInfo("Stopping camera manager");

    // Clean up camera manager
    if (cameraManager_)
    {
        cameraManager_->stop();
        cameraManager_.reset();
    }

    // Cleanup any lingering IPA processes manually for now... this should be
    // handled by the camera agent's closeCamera() method in the future.
    const int result = system("pkill -f raspberrypi_ipa 2>/dev/null");
}
} // namespace PiTrac