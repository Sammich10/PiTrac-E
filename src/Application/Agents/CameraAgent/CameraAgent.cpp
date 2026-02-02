#include "Application/Agents/CameraAgent/CameraAgent.h"
#include "Interfaces/Camera/GSCameraBase/GSCameraBase.h"
#include <libcamera/camera_manager.h>
#include <thread>
#include <semaphore>
#include <future>

namespace PiTrac
{
CameraAgent::CameraAgent(const size_t camera_index, const std::string &process_name)
    : AgentBase(process_name)
    , frame_buffer_(std::make_shared<FrameBuffer>(64)) // Default buffer size of
                                                       // 64 frames
    , camera_(nullptr)
    , distortion_calibrator_(nullptr)
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
    // Instantiate the camera object
    camera_ = std::make_unique<GSCameraBase>(camera_index_, camera_manager);
    if(camera_ == nullptr)
    {
        logError("Failed to create camera interface for: " + name_);
        return false;
    }
    // Instantiate the frame publisher
    frame_publisher_ = std::make_unique<MessagerBase>(MessagerBase::SocketType::Push);
    if(frame_publisher_ == nullptr)
    {
        logError("Failed to create frame publisher for: " + name_);
        return false;
    }
    try{
        // Create a PUSH messager to send frames to the frame collection
        // endpoint
        frame_publisher_->connect(Endpoints::getFrameCollectionEndpoint());
    } catch (const std::exception &e) {
        logError("Exception connecting frame publisher for: " + name_ + " - " + std::string(e.what()));
        return false;
    }
    // Initialize frame codec (JPEG for viewfinder) TODO: make this dynamic
    // later
    frame_codec_ = std::make_unique<JpegCodec>();
    if(frame_codec_ == nullptr)
    {
        logError("Failed to create frame codec for: " + name_);
        return false;
    }
    // TODO: Load codec params from config later
    frame_codec_params_ = { { {"quality", "90"} } };
    return true;
}

bool CameraAgent::changeMode(PiTrac::SystemMode_Type new_mode)
{
    cleanUp(); // Ensure previous mode is cleaned up
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
                cleanUp();
                return false;
            }
            if(!camera_->startContinuousCapture(std::bind(&CameraAgent::viewfinderCallback, this, std::placeholders::_1)))
            {
                logError("Failed to start continuous capture for: " + name_);
                return false;
            }
            return true;
        case SystemMode_Type::CALIBRATION:
            logInfo("Starting calibration mode for: " + name_);
            if(!configureCalibration())
            {
                logError("Failed to configure calibration for: " + name_);
                cleanUp();
                return false;
            }
            break;
        default:
            logInfo("Unimplemented mode for CameraAgent: " + std::to_string(static_cast<int>(new_mode)));
            return false;
    }
    logInfo("Mode change complete to " + std::to_string(static_cast<int>(lm_mode_)) + " for: " + name_);
    return true;
}

bool CameraAgent::handleSystemCommand(const SystemCommandMsg &command_msg)
{
    // Check the command type
    const SystemCommandMsg::CommandID cmd = static_cast<SystemCommandMsg::CommandID>(command_msg.getCommand_id());
    // Check if this is a calibration command
    switch(cmd)
    {
        case SystemCommandMsg::CommandID::Calibrate:
        {
            if(lm_mode_ != SystemMode_Type::CALIBRATION)
            {
                logWarning("Received calibration command while not in CALIBRATION mode for: " + name_);
                return false;
            }
            logInfo("Processing calibration command for: " + name_);
            return processCalibrationCommand(command_msg.getCommand_params());
        }
        default:
            logWarning("Unknown or unimplemented SystemCommandMsg command ID: " + std::to_string(static_cast<int>(cmd)) + " for: " + name_);
            return false;
    }
}

bool CameraAgent::configureViewfinder()
{
    logInfo("Configuring viewfinder mode for: " + name_);
    // TODO: Load these settings from a config file or parameters later
    camera_->setResolution(1456 / 2, 1088 / 2); // Half resolution for
                                                // viewfinder
    camera_->setExposureTime(20000); // 20ms - increased from 2ms for better
                                     // exposure
    camera_->setSensorSize(3.674f, 2.760f); // IMX219 sensor size in mm
    camera_->setFrameRate(10.0f); // 10 FPS
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
    if (!camera_->openCamera())
    {
        logError("Failed to open camera for: " + name_);
        return false;
    }
    if(!camera_->configureStream(libcamera::StreamRole::VideoRecording))
    {
        logError("Failed to configure camera stream for: " + name_);
        return false;
    }
    if(!camera_->start())
    {
        logError("Failed to start camera for: " + name_);
        return false;
    }
    frame_counter_ = 0; // Reset frame counter
    return true;
}

void CameraAgent::viewfinderCallback(cv::Mat &frame)
{
    // Stream the captured frame. No other functionality needed here for
    // viewfinder.
    streamFrame(frame);
    frame_counter_++;
}

bool CameraAgent::configureCalibration()
{
    // TODO: Load these settings from a config file or parameters later
    // Configure full resolution for calibration
    camera_->setResolution(1456, 1088);
    const double exposure_ms = 1 / 30.0 * 1000.0; // 1/30s in ms
    camera_->setExposureTime(static_cast<uint32_t>(exposure_ms * 1000)); // Convert
                                                                         // ms
                                                                         // to
                                                                         // us
    camera_->setSensorSize(3.674f, 2.760f); // IMX219 sensor size in mm
    camera_->setFrameRate(30.0f); // 30 FPS - faster response for still capture
    camera_->setAnalogGain(6.0f); // Increased gain for better brightness
    camera_->setFocalLength(2.8f);
    camera_->setNumBuffers(1); // Single buffer for still capture
    camera_->setTriggerMode(TriggerMode::FREE_RUNNING);
    // Ensure the camera object was created
    if(camera_ == nullptr)
    {
        logError("Camera interface is null for: " + name_);
        return false;
    }
    // If the camera was not closed properly before, close it now
    if(camera_->isCameraOpen())
    {
        logInfo("Camera already open, closing for re-initialization: " + name_);
        camera_->closeCamera();
    }
    // Open or re-open the camera
    logInfo("Opening camera for: " + name_);
    if (!camera_->openCamera())
    {
        logError("Failed to open camera for: " + name_);
        return false;
    }
    // Configure the camera stream for still capture
    if(!camera_->isCameraConfigured())
    {
        logInfo("Initializing camera for: " + name_);
        if(!camera_->configureStream(libcamera::StreamRole::StillCapture))
        {
            logError("Failed to configure camera stream for: " + name_);
            return false;
        }
    }
    if(!camera_->start())
    {
        logError("Failed to start camera for: " + name_);
        return false;
    }
    if(!camera_->startContinuousCapture(nullptr))
    {
        logError("Failed to start continuous capture for: " + name_);
        return false;
    }
    // Instantiate distortion calibrator TODO: make this dynamic later, maybe instantiate on demand
    distortion_calibrator_ = std::make_unique<CheckerboardCalibration>();
    if(distortion_calibrator_ == nullptr)
    {
        logError("Failed to create distortion calibrator for: " + name_);
        return false;
    }
    frame_counter_ = 0; // Reset frame counter

    return true;
}

bool CameraAgent::processCalibrationCommand(const std::map<std::string, std::string> &commandParams)
{
    logInfo(name_ + ": Processing calibration command ID");

    // Parse calibration parameters from command
    if (commandParams.find("action") != commandParams.end())
    {
        std::string action = commandParams.at("action");

        if (action == "capture_image")
        {
            // if(!camera_->start())
            // {
            //     logError("Failed to start camera for: " + name_);
            //     return false;
            // }
            logInfo(name_ + ": Capturing calibration image");
            cv::Mat calibration_frame = camera_->captureFrame(5000); // 5 second
                                                                     // timeout
            // if(!camera_->stop())
            // {
            //     logError("Failed to stop camera after capture for: " +
            // name_);
            //     return false;
            // }

            if (!calibration_frame.empty())
            {
                // Store in frame buffer for processing
                frame_buffer_->addFrame(calibration_frame);
                frame_counter_ = frame_buffer_->size();
                streamFrame(calibration_frame); // Stream captured calibration
                                                // image
                logInfo(name_ + ": Calibration image captured successfully");
                return true;
            }
            else
            {
                logError(name_ + ": Failed to capture calibration image - empty frame returned");
                return false;
            }
        }
        else if(action == "do_distortion_cal")
        {
            logInfo(name_ + ": Performing distortion calibration");
            // Retrieve all frames from the buffer for calibration
            std::vector<cv::Mat> calibration_frames(0);
            cv::Mat frame;
            while(frame_buffer_->getFrame(frame))
            {
                calibration_frames.push_back(frame);
            }
            if(calibration_frames.empty())
            {
                logError(name_ + ": No frames available for distortion calibration");
                return false;
            }
            distortion_calibrator_->setDimensions(6, 7);
            distortion_calibrator_->setImages(calibration_frames);
            if(!distortion_calibrator_->doDistortionCalibration())
            {
                logError(name_ + ": Distortion calibration failed");
                return false;
            }
            std::vector<cv::Mat> debug_images = distortion_calibrator_->getDebugImages();
            // Stream debug images if available
            for(const auto &dbg_img : debug_images)
            {
                streamFrame(dbg_img);
            }
            // Clear the frame buffer after calibration
            frame_buffer_->clear();
            distortion_calibrator_->clearImages();
            // Perform distortion calibration using the collected frames
            // This is a placeholder - actual calibration logic would go here
            logInfo(name_ + ": Distortion calibration completed with " + std::to_string(calibration_frames.size()) + " frames");
            return true;
        }
        // else if (action == "start_preview")
        // {
        //     logInfo(name_ + ": Starting calibration preview mode");
        //     // Configure for preview and start continuous capture
        //     // Implementation depends on your preview requirements
        // }
        // else if (action == "stop_preview") {
        //     logInfo(name_ + ": Stopping calibration preview mode");
        //     if (camera_->isCameraOpen()) {
        //         camera_->stopContinuousCapture();
        //     }
        //     return true;
        // }
        else
        {
            logError(name_ + ": Unknown calibration action: " + action);
            return false;
        }
    }

    // Unknown or unhandled command
    logWarning(name_ + ": Unknown calibration command action");
    return true;
}

void CameraAgent::streamFrame(const cv::Mat &frame)
{
    // Encode the frame using the configured codec
    std::vector<uint8_t> encoded_data = frame_codec_->encode(frame, frame_codec_params_);

    // Check if encoding was successful
    if (encoded_data.empty())
    {
        logWarning("Failed to encode frame for camera " + std::to_string(camera_index_) + " using codec: " + frame_codec_->getCodecName());
        return;
    }
    // Create a frame message and push it
    CameraFrameMsg frame_msg(
        "Camera_" + std::to_string(camera_index_),
        frame_counter_,
        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                  std::chrono::system_clock::now().time_since_epoch()).count()),
        camera_->getFrameRate(),
        encoded_data,
        { {"Codec", "JPEG"}, {"Quality", "90"} }
        );
    frame_publisher_->sendMessage(frame_msg);
}

void CameraAgent::cleanUp()
{
    logInfo("Cleaning up: " + name_);
    // Stop current operations
    run_.store(false);
    // Wait for current thread to finish
    if(agent_thread_.joinable())
    {
        agent_thread_.join();
    }
    frame_buffer_->clear();
    // Close camera if open to release resources and prepare for new mode
    if(camera_)
    {
        try {
            if (camera_->isCameraOpen())
            {
                // Stop any ongoing capture first
                if (camera_->isCameraCapturing())
                {
                    logInfo("Stopping continuous capture for: " + name_);
                    camera_->stopContinuousCapture();
                }
                logInfo("Closing camera for: " + name_);
                camera_->stop();
                camera_->closeCamera();
            }
        } catch (const std::exception &e) {
            logError("Exception during camera cleanup: " + std::string(e.what()));
        }
    }

    // Reset frame counter
    frame_counter_ = 0;
    // Stream a blank/black frame to indicate mode change / stop
    cv::Mat black_frame = cv::Mat::zeros(camera_->getResolutionY(), camera_->getResolutionX(), camera_->getPixelFormat());
    streamFrame(black_frame);

    logInfo("Cleanup completed: " + name_);
}

void CameraAgent::cleanupProcess()
{
    // Call the internal cleanup function
    cleanUp();
    // Disconnect the frame publisher
    if(frame_publisher_)
    {
        frame_publisher_->disconnect(Endpoints::getFrameCollectionEndpoint());
    }
}

} // namespace PiTrac