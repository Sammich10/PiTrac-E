#include "Application/AppAgents/CameraAgent/CameraAgent.h"

namespace PiTrac
{
CameraAgent::CameraAgent(std::unique_ptr<GSCameraInterface> camera_,
                         const std::string &camera_id,
                         const std::string &endpoint)
    : GSTask("CameraAgent_" + camera_id, TaskPriority::High),
    camera_(std::move(camera_)),
    camera_id_(camera_id),
    endpoint_(endpoint),
    running_(false),
    frame_counter_(0),
    current_mode_(SystemMode::Initializing)
{
    publisher_ = std::make_unique<GSMessagerBase>(GSMessagerBase::SocketType::Publisher);
    publisher_->bind(endpoint_);
    subscriber_ = std::make_unique<GSMessagerBase>(GSMessagerBase::SocketType::Subscriber);
    subscriber_->connect(endpoint_);
}

CameraAgent::~CameraAgent()
{
    cleanup();
    logger_->info("CameraAgent destroyed: " + camera_id_);
}

bool CameraAgent::initialize()
{
    logger_->info("Initializing CameraAgent for camera ID: " + camera_id_);
    if(!camera_->isCameraOpen())
    {
        logger_->info("Opening camera for CameraAgent with ID: " + camera_id_);
        if (!camera_->openCamera(0))
        {
            logger_->error("Failed to open camera for CameraAgent with ID: " + camera_id_);
            return false;
        }
    }
    if(!camera_->isCameraConfigured())
    {
        logger_->info("Initializing camera for CameraAgent with ID: " + camera_id_);
        if (!camera_->initializeCamera())
        {
            logger_->error("Failed to initialize camera for CameraAgent with ID: " + camera_id_);
            return false;
        }
    }
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
        logger_->error("Failed to start continuous capture for CameraAgent with ID: " + camera_id_);
        return;
    }

    while(!shouldStop())
    {
        cv::Mat frame = camera_->getNextFrame();
        if(frame.empty())
        {
            logger_->warning(
                "Received empty frame from camera for CameraAgent with ID: " + camera_id_);
            continue;
        }
        // Create message on the stack to avoid heap allocation overhead and
        // fragmentation in high-frequency scenarios like camera frame capture.
        GSCameraFrameRawMessage message(camera_id_, frame, frame_counter_++);
        publisher_->sendMessage(message, "CameraFrameRaw");
        logger_->info("Published frame number " + std::to_string(
                          frame_counter_ - 1) + " from CameraAgent with ID: " + camera_id_);
    }
}

void CameraAgent::cleanup()
{
    logger_->info("Cleaning up CameraAgent for camera ID: " + camera_id_);
    if (camera_ && camera_->isCameraOpen())
    {
        camera_->closeCamera();
    }
    if (capture_thread_.joinable())
    {
        running_ = false;
        capture_thread_.join();
    }
    logger_->info("CameraAgent cleanup completed for camera ID: " + camera_id_);
}
}