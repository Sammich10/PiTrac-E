#include "Application/Agents/CameraAgent/CameraAgent.h"
#include "Interfaces/Camera/GSCameraBase/GSCameraBase.h"
#include <libcamera/camera_manager.h>

namespace PiTrac
{
CameraAgent::CameraAgent(const size_t camera_index)
    : AgentBase("CameraAgent_" + std::to_string(camera_index))
    , frame_buffer_(std::make_shared<FrameBuffer>(64)) // Default buffer size of
                                                       // 64 frames
    , frame_processor_(nullptr)
    , camera_(nullptr)
    , camera_index_(camera_index)
    , running_(false)
    , frame_counter_(0)
{
    logInfo("CameraAgent created: " + name_);
}

CameraAgent::~CameraAgent()
{
    logInfo("CameraAgent destroyed: " + name_);
}

bool CameraAgent::setupProcess()
{
    logInfo("Setting up " + name_);

    std::shared_ptr<libcamera::CameraManager> camera_manager = std::make_shared<libcamera::CameraManager>();
    const int ret = camera_manager->start();
    if (ret)
    {
        logError("Failed to start camera manager for: " + name_);
        return false;
    }

    camera_ = std::make_unique<GSCameraBase>(camera_index_, camera_manager);
    camera_->setResolution(1456, 1088);
    camera_->setExposureTime(2000); // 2ms
    camera_->setSensorSize(3.674f, 2.760f); // IMX219 sensor size in mm
    camera_->setFrameRate(15.0f); // 15 FPS
    camera_->setFocalLength(2.8f);
    camera_->setTriggerMode(TriggerMode::FREE_RUNNING);

    logInfo("Initializing CameraAgent for: " + name_);

    if(!camera_->isCameraOpen())
    {
        logInfo("Opening camera for: " + name_);
        if (!camera_->openCamera())
        {
            logError("Failed to open camera for: " + name_);
            return false;
        }
    }

    if(!camera_->isCameraConfigured())
    {
        logInfo("Initializing camera for: " + name_);
        if(!camera_->configureStream(libcamera::StreamRole::VideoRecording))
        {
            logError("Failed to configure camera stream for: " + name_);
            return false;
        }
    }

    return true;
}

void CameraAgent::changeMode(PiTrac::SystemMode_Type new_mode)
{
    logInfo("Received mode change request to " + std::to_string(static_cast<int>(new_mode)) + " for: " + name_);
    std::lock_guard<std::mutex> lock(mode_mutex_);
    if(lm_mode_ == new_mode)
    {
        logInfo("Mode is already " + std::to_string(static_cast<int>(lm_mode_)) + " for: " + name_);
        return;
    }
    lm_mode_ = new_mode;
    logInfo("Changing mode to " + std::to_string(static_cast<int>(lm_mode_)) + " for: " + name_);
    run_.store(false);
    if(agent_thread_.joinable())
    {
        agent_thread_.join();
    }
    run_.store(true);

    switch(new_mode)
    {
        case SystemMode_Type::VIEWFINDER:
            logInfo("Starting viewfinder mode for: " + name_);
            agent_thread_ = std::thread(&CameraAgent::viewfinderLoop, this);
            // Initialize and start frame processor
            if(!frame_processor_)
            {
                frame_processor_ = FrameProcessorFactory::create(FrameProcessor::ProcessorType::STREAMING, frame_buffer_, camera_index_);
                if(!frame_processor_)
                {
                    logError("Failed to create FrameProcessor for: " + name_);
                    run_.store(false);
                    break;
                }
                if(!frame_processor_->init())
                {
                    logError("Failed to initialize FrameProcessor for: " + name_);
                    run_.store(false);
                    break;
                }
            }
            frame_processor_->start();
            break;
        default:
            logInfo("Unimplemented mode for CameraAgent: " + std::to_string(static_cast<int>(new_mode)) + " for: " + name_);
            run_.store(false);
            break;
    }
    logInfo("Mode change complete to " + std::to_string(static_cast<int>(lm_mode_)) + " for: " + name_);
}

void CameraAgent::viewfinderLoop()
{
    if(!camera_->startContinuousCapture())
    {
        logError("Failed to start continuous capture for: " + name_);
        return;
    }

    while(run_.load() && !should_stop_.load())
    {
        cv::Mat frame = camera_->getNextFrame();
        if(frame.empty())
        {
            logWarning("Received empty frame from camera for: " + name_);
            continue;
        }
        frame_buffer_->addFrame(frame);
        frame_counter_++;
    }
    logInfo("Stopping continuous capture for: " + name_);
    camera_->stopContinuousCapture();
    logInfo("CameraAgent capture loop exiting for: " + name_);
}

void CameraAgent::cleanupProcess()
{
    logInfo("Cleaning up CameraAgent for: " + name_);
    if (camera_ && camera_->isCameraOpen())
    {
        camera_->closeCamera();
    }
    logInfo("CameraAgent cleanup completed for: " + name_);
}
}