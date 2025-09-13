#include "Application/Tasks/CameraTask/CameraTask.h"
#include "Interfaces/Camera/imx296/imx296.h"

namespace PiTrac
{
CameraTask::CameraTask(const size_t camera_index, const size_t frame_buffer_size)
    : GSAgentTask("CameraTask")
    , camera_index_(camera_index)
    , frame_buffer_size_(frame_buffer_size)
{
    setRestartFailedAgents(true);
    setAgentCheckInterval(std::chrono::milliseconds(2000));
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
    // Subscribe to agent task IPC endpoint to receive commands from the system
    // regarding
    // state updates
    agent_task_ipc_subscriber_->bind(agent_task_ipc_endpoint_);
    logInfo("Agent task IPC subscriber bound to: " + agent_task_ipc_endpoint_);
    return true;
}

void CameraTask::configureAgents()
{
    logger_->info("Configuring Camera Task Agents...");
    // Create a camera and frame buffer for the camera
    std::unique_ptr<GSCameraInterface> camera = std::make_unique<IMX296Camera>(camera_index_, cameraManager_);
    std::shared_ptr<FrameBuffer> frame_buffer = std::make_shared<FrameBuffer>(frame_buffer_size_);
    // Create and add camera agent and frame processor agent
    addAgent(CameraAgentFactory::createCameraAgent(std::move(camera), frame_buffer, camera_index_));
    addAgent(FrameProcessorAgentFactory::create(std::move(frame_buffer), camera_index_));

    logger_->info("Configured camera task for camera " + std::to_string(camera_index_) + ".");
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
    // const int result = system("pkill -f raspberrypi_ipa 2>/dev/null");
}
} // namespace PiTrac