#include "Application/Agents/CameraAgent/CameraAgent.h"
#include "Interfaces/Camera/GSCameraBase/GSCameraBase.h"
#include "Common/Utils/CodecUtils/CodecUtils.h"
#include <libcamera/camera_manager.h>
#include <thread>

namespace PiTrac
{
CameraAgent::CameraAgent(const size_t camera_index)
    : AgentBase("CameraAgent_" + std::to_string(camera_index))
    , frame_buffer_(std::make_shared<FrameBuffer>(64)) // Default buffer size of
                                                       // 64 frames
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
    return true;
}

void CameraAgent::changeMode(PiTrac::SystemMode_Type new_mode)
{
    cleanupProcess(); // Ensure previous mode is cleaned up
    switch(new_mode)
    {
        case SystemMode_Type::STANDBY:
            logInfo("Entering standby mode for: " + name_);
            // Nothing to do, just ensure camera is closed and idle, and we are
            // ready to switch modes
            break;

        case SystemMode_Type::VIEWFINDER:
            logInfo("Starting viewfinder mode for: " + name_);
            if(!configureViewfinder())
            {
                logError("Failed to configure viewfinder for: " + name_);
                cleanupProcess();
                return;
            }
            // Reset run flag for new thread
            run_.store(true);
            // Start viewfinder thread
            agent_thread_ = std::thread(&CameraAgent::startViewfinder, this);
            break;
        default:
            logInfo("Unimplemented mode for CameraAgent: " + std::to_string(static_cast<int>(new_mode)));
            break;
    }
    logInfo("Mode change complete to " + std::to_string(static_cast<int>(lm_mode_)) + " for: " + name_);
}

bool CameraAgent::configureViewfinder()
{
    // TODO: Load these settings from a config file or parameters later
    camera_->setResolution(1456, 1088);
    camera_->setExposureTime(20000); // 20ms - increased from 2ms for better
                                     // exposure
    camera_->setSensorSize(3.674f, 2.760f); // IMX219 sensor size in mm
    camera_->setFrameRate(15.0f); // 15 FPS
    camera_->setAnalogGain(4.0f); // Increased gain for better brightness
    camera_->setFocalLength(2.8f);
    camera_->setTriggerMode(TriggerMode::FREE_RUNNING);

    // Open the camera if not already open
    if(camera_ == nullptr)
    {
        logError("Camera interface is null for: " + name_);
        return false;
    }

    if(camera_->isCameraOpen())
    {
        logInfo("Camera already open, closing for re-initialization: " + name_);
        camera_->closeCamera();
    }

    logInfo("Opening camera for: " + name_);
    if (!camera_->openCamera())
    {
        logError("Failed to open camera for: " + name_);
        return false;
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

    if(!camera_->startContinuousCapture())
    {
        logError("Failed to start continuous capture for: " + name_);
        return false;
    }

    return true;
}

void CameraAgent::startViewfinder()
{
    logInfo(name_ + ": Starting viewfinder loop");
    // TODO: at some point, the codec here should be dynamic based on config
    JpegCodec codec;
    // Create a PUSH messager to send frames to the frame collection endpoint
    std::unique_ptr<MessagerBase> frame_publisher_ = std::make_unique<MessagerBase>(MessagerBase::SocketType::Push);
    frame_publisher_->connect(Endpoints::getFrameCollectionEndpoint());
    // Frame counter for this session
    uint64_t frame_count_ = 0;
    // Calculate FPS based on camera settings
    float fps_ = camera_->getFrameRate();
    logInfo(name_ + ": Viewfinder loop started with target FPS: " + std::to_string(fps_));
    while(run_.load() && !should_stop_.load())
    {
        // Get the next frame from the camera
        cv::Mat frame = camera_->getNextFrame();
        if (frame.empty() || frame.rows == 0 || frame.cols == 0)
        {
            logWarning("Received empty frame from camera for: " + name_);
            // Add small delay when no frame available to prevent tight loop
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            // Skip invalid frames
            continue;
        }

        // Additional validation - check if frame has valid data. Shouldn't be
        // necessary, but
        // just in case.
        if (frame.type() == CV_8UC1 || frame.type() == CV_8UC3 || frame.type() == CV_8UC4)
        {
            auto encoded_data = codec.encode(frame, CodecParams{ { {"quality", "90"} } });

            // Check if encoding was successful
            if (encoded_data.empty())
            {
                logWarning("Failed to encode frame for camera " + std::to_string(camera_index_) + " using codec: " + codec.getCodecName());
                continue;
            }
            // Create a frame message and push it
            CameraFrameMsg frame_msg(
                "Camera_" + std::to_string(camera_index_),
                frame_count_++,
                static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                          std::chrono::system_clock::now().time_since_epoch()).count()),
                fps_,
                encoded_data,
                { {"Codec", "JPEG"}, {"Quality", "90"} }
                );
            frame_publisher_->sendMessage(frame_msg);
        }
        else
        {
            // Unsupported frame type, log warning TODO: remove after debugging
            static int warning_count = 0;
            if (warning_count++ < 5) // Limit log spam
            {
                logWarning("Warning: Unsupported frame type: " + std::to_string(frame.type()) + " for camera " + std::to_string(camera_index_));
            }
        }
        // Yield CPU briefly to allow other threads to run
        std::this_thread::yield();
    }
}

void CameraAgent::cleanupProcess()
{
    logInfo("Cleaning up: " + name_);
    // Stop current operations
    run_.store(false);
    // Wait for current thread to finish
    if(agent_thread_.joinable())
    {
        agent_thread_.join();
    }
    // Close camera if open to release resources and prepare for new mode
    if(camera_ && camera_->isCameraOpen())
    {
        camera_->closeCamera();
    }
    logInfo("Cleanup completed: " + name_);
}
}