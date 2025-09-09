#include "Application/AppAgents/CameraAgent/CameraAgent.h"

namespace PiTrac
{
CameraAgent::CameraAgent(std::unique_ptr<GSCameraInterface> camera_, const std::string &endpoint)
    : GSAgentBase("CameraAgent", AgentPriority::High),
    camera_(std::move(camera_)),
    camera_id_(0),
    endpoint_(endpoint),
    running_(false),
    frame_counter_(0),
    current_mode_(SystemMode::Initializing)
{
    frame_publisher_ = std::make_unique<GSMessagerBase>(GSMessagerBase::SocketType::Publisher);
    subscriber_ = std::make_unique<GSMessagerBase>(GSMessagerBase::SocketType::Subscriber);
}

CameraAgent::~CameraAgent()
{
    cleanup();
    logger_->info("CameraAgent destroyed: " + agent_name_);
}

bool CameraAgent::setup()
{
    logger_->info("Setting up CameraAgent for: " + agent_name_);
    camera_id_ = camera_->getCameraIndex();
    agent_name_ = agent_name_ + "_" + std::to_string(camera_id_);
    camera_->setResolution(1456, 1088);
    camera_->setFocalLength(2.8f);
    camera_->setTriggerMode(TriggerMode::FREE_RUNNING);
    return true;
}

bool CameraAgent::initialize()
{
    logger_->info("Initializing CameraAgent for: " + agent_name_);
    if(!camera_->isCameraOpen())
    {
        logger_->info("Opening camera for: " + agent_name_);
        if (!camera_->openCamera())
        {
            logger_->error("Failed to open camera for: " + agent_name_);
            return false;
        }
    }
    if(!camera_->isCameraConfigured())
    {
        logger_->info("Initializing camera for: " + agent_name_);
        if (!camera_->initializeCamera())
        {
            logger_->error("Failed to initialize camera for: " + agent_name_);
            return false;
        }
    }
    // ZMQ setup last (after camera is working)
    try {
        frame_publisher_->bind(endpoint_);
        // subscriber_->connect(endpoint_); // Uncomment if subscribing to
        // another endpoint is needed. For now, we only publish frames.
    } catch (const std::exception &e) {
        logger_->error("ZMQ initialization failed: " + std::string(e.what()));
        return false;
    }

    logger_->info("ZMQ messaging initialized for: " + agent_name_);

    return true;
}

void CameraAgent::execute()
{
    captureLoop();
}

void CameraAgent::captureLoop()
{
    if(!camera_->startContinuousCapture())
    {
        logger_->error("Failed to start continuous capture for: " + agent_name_);
        return;
    }

    while(!shouldStop())
    {
        cv::Mat frame = camera_->getNextFrame();
        if(frame.empty())
        {
            logger_->warning(
                "Received empty frame from camera for: " + agent_name_);
            continue;
        }
        // Create message on the stack to avoid heap allocation overhead and
        // fragmentation in high-frequency scenarios like camera frame capture.
        GSCameraFrameRawMessage message(camera_id_, frame, frame_counter_++);
        frame_publisher_->sendMessage(message, "CameraFrame");
    }

    logger_->info("CameraAgent capture loop exiting for: " + agent_name_);
    camera_->stopContinuousCapture();
}

void CameraAgent::cleanup()
{
    logger_->info("Cleaning up CameraAgent for: " + agent_name_);
    if (camera_ && camera_->isCameraOpen())
    {
        camera_->closeCamera();
    }
    if (capture_thread_.joinable())
    {
        running_ = false;
        capture_thread_.join();
    }
    logger_->info("CameraAgent cleanup completed for: " + agent_name_);
}
}