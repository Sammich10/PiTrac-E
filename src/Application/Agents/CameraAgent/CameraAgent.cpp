#include "Application/Agents/CameraAgent/CameraAgent.h"

namespace PiTrac
{
CameraAgent::CameraAgent(std::unique_ptr<GSCameraInterface> camera_, std::shared_ptr<FrameBuffer> frame_buffer_, const uint32_t camera_id_)
    : GSAgentBase("CameraAgent", AgentPriority::High),
    camera_(std::move(camera_)),
    frame_buffer_(std::move(frame_buffer_)),
    camera_id_(camera_id_),
    running_(false),
    frame_counter_(0),
    current_mode_(SystemMode::Initializing)
{
    agent_name_ = agent_name_ + " " + std::to_string(camera_id_);
    logInfo("CameraAgent created: " + agent_name_);
}

CameraAgent::~CameraAgent()
{
    cleanup();
    logInfo("CameraAgent destroyed: " + agent_name_);
}

bool CameraAgent::setup()
{
    logInfo("Setting up " + agent_name_);
    camera_->setResolution(1456, 1088);
    camera_->setFocalLength(2.8f);
    camera_->setTriggerMode(TriggerMode::FREE_RUNNING);
    return true;
}

bool CameraAgent::initialize()
{
    logInfo("Initializing CameraAgent for: " + agent_name_);
    if(!camera_->isCameraOpen())
    {
        logInfo("Opening camera for: " + agent_name_);
        if (!camera_->openCamera())
        {
            logError("Failed to open camera for: " + agent_name_);
            return false;
        }
    }
    if(!camera_->isCameraConfigured())
    {
        logInfo("Initializing camera for: " + agent_name_);
        if (!camera_->initializeCamera())
        {
            logError("Failed to initialize camera for: " + agent_name_);
            return false;
        }
    }

    return true;
}

void CameraAgent::execute()
{
    logInfo("CameraAgent execution started for: " + agent_name_);
    captureLoop();
}

void CameraAgent::captureLoop()
{
    if(!camera_->startContinuousCapture())
    {
        logError("Failed to start continuous capture for: " + agent_name_);
        return;
    }

    while(!shouldStop())
    {
        cv::Mat frame = camera_->getNextFrame();
        if(frame.empty())
        {
            logWarning("Received empty frame from camera for: " + agent_name_);
            continue;
        }
        frame_buffer_->addFrame(frame);
        frame_counter_++;
    }
    logInfo("Stopping continuous capture for: " + agent_name_);
    camera_->stopContinuousCapture();
    logInfo("CameraAgent capture loop exiting for: " + agent_name_);
}

void CameraAgent::cleanup()
{
    logInfo("Cleaning up CameraAgent for: " + agent_name_);
    if (camera_ && camera_->isCameraOpen())
    {
        camera_->closeCamera();
    }
    logInfo("CameraAgent cleanup completed for: " + agent_name_);
}
}