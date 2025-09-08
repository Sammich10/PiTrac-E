#include "Application/AppAgents/CameraAgent/CameraAgent.h"

namespace PiTrac
{
CameraAgent::CameraAgent(std::unique_ptr<GSCameraInterface> camera_,
                         const std::string &camera_id,
                         const std::string &endpoint)
    : GSAgentBase("CameraAgent_" + camera_id, AgentPriority::High),
    camera_(std::move(camera_)),
    camera_id_(camera_id),
    endpoint_(endpoint),
    running_(false),
    frame_counter_(0),
    current_mode_(SystemMode::Initializing)
{
    publisher_ = std::make_unique<GSMessagerBase>(GSMessagerBase::SocketType::Publisher);
    subscriber_ = std::make_unique<GSMessagerBase>(GSMessagerBase::SocketType::Subscriber);
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
    // ZMQ setup last (after camera is working)
    try {
        publisher_->bind(endpoint_);
        subscriber_->connect(endpoint_);
    } catch (const std::exception &e) {
        logger_->error("ZMQ initialization failed: " + std::string(e.what()));
        return false;
    }

    logger_->info("ZMQ messaging initialized for CameraAgent with ID: " + camera_id_);

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
        // logger_->info("Published frame number " + std::to_string(
        //                   frame_counter_ - 1) + " from CameraAgent with ID: "
        // + camera_id_);
    }

    logger_->info("CameraAgent capture loop exiting for camera ID: " + camera_id_);
    camera_->stopContinuousCapture();
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