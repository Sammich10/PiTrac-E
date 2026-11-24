#include "Application/Agents/FlightProcessor/FlightProcessor.h"
#include "Interfaces/Camera/GSCameraBase/GSCameraBase.h"
#include "Common/Utils/Calibration/CalibrationUtils.h"
#include "Common/Utils/Detection/BallDetectionUtils.h"
#include "Common/Math/Constants.h"
#include <thread>
#include <semaphore>
#include <future>

namespace PiTrac
{
FlightProcessor::FlightProcessor(const std::string &process_name)
    : AgentBase(process_name)
    , current_calibration_model_(CalibrationModel::FISHEYE)
{
    logInfo("FlightProcessor created: " + name_);
    for(uint32_t cam = 0; cam < static_cast<uint32_t>(LMCameras::NUM_CAMERAS); ++cam)
    {
        camera_[cam] = nullptr; // Initialize camera pointer to null
        frame_buffer_[cam] = std::make_unique<FrameBuffer>(4); // Initialize frame buffer pointer with default size
        pause_stream_[cam] = false; // Initialize pause stream flag
        apply_calibrations_to_viewfinder_[cam] = false; // Initialize apply calibrations to viewfinder flag
        frame_counter_[cam] = 0; // Initialize frame counter
    }
    camera_manager_ = nullptr; // Initialize camera manager pointer to null
    distortion_calibrator_ = nullptr; // Initialize distortion calibrator pointer to null
}

FlightProcessor::~FlightProcessor()
{
    cleanUp();
    logInfo("FlightProcessor destroyed: " + name_);
}

bool FlightProcessor::setupProcess()
{
    logInfo("Setting up " + name_);
    // Instantiate the libcamera camera manager and start it
    camera_manager_ = std::make_shared<libcamera::CameraManager>();
    const int ret = camera_manager_->start();
    if (ret)
    {
        logError("Failed to start camera manager for: " + name_ + ". Error code: " + std::to_string(ret));
        return false;
    }
    else if(camera_manager_->cameras().empty() || camera_manager_->cameras().size() < static_cast<uint32_t>(LMCameras::NUM_CAMERAS))
    {
        logError("Not enough cameras available for: " + name_);
        return false;
    }
    // Setup the camera objects
    for(uint32_t cam = 0; cam < static_cast<uint32_t>(LMCameras::NUM_CAMERAS); ++cam)
    {
        // Instantiate the camera object
        camera_[cam] = std::make_unique<GSCameraBase>(cam, camera_manager_);
        if(camera_[cam] == nullptr)
        {
            logError("Failed to create camera interface for: " + name_);
            return false;
        }
        // Initialize the camera
        if(!camera_[cam]->initialize())
        {
            logError("Failed to initialize camera interface for: " + name_);
            return false;
        }
        // Load and validate camera UUID and basic info
        if(!camera_[cam]->getUUIDInfo().isValid())
        {
            logError("Invalid camera UUID info for: " + name_);
            return false;
        }
        // Configure the camera stream with resolution, frame rate, and trigger mode
        configureCameraStream(camera_[cam], 1456, 1088, 10.0f, TriggerMode::FREE_RUNNING);
        // Configure camera constants such as focal length and sensor size. This should eventually come from a config file
        configureCameraConstants(camera_[cam], 2.8f, 3.674f, 2.760f);
        // Load dynamic camera settings from the calibration database (like exposure time, gain, etc) and apply
        loadCameraSettings(camera_[cam]);
    }
    // Initialize frame codec (JPEG for viewfinder) TODO: make this dynamic later
    frame_codec_ = std::make_unique<JpegCodec>();
    if(frame_codec_ == nullptr)
    {
        logError("Failed to create frame codec for: " + name_);
        return false;
    }
    // TODO: Load codec params from config later
    frame_codec_params_ = { { {"quality", "90"} } };
    return AgentBase::setupProcess();
}

bool FlightProcessor::handleChangeMode(const SystemCommandMsg *command_msg)
{
    cleanUp(); // Ensure previous mode is cleaned up
    const SystemCommandMsg::SetModeAction action = static_cast<const SystemCommandMsg::SetModeAction>(command_msg->getAction());
    bool mode_changed = false;
    switch(action)
    {
        case SystemCommandMsg::SetModeAction::EnterStandbyMode:
            logInfo("Entering standby mode...");
            mode_changed = configureStandby();
            break;
        case SystemCommandMsg::SetModeAction::EnterCalibrationMode:
            logInfo("Entering calibration mode...");
            mode_changed = configureCalibration();
            break;
        case SystemCommandMsg::SetModeAction::EnterLaunchMonitorMode:
            logInfo("Entering launch monitor mode...");
            mode_changed = configureLaunchMonitor();
            break;
        case SystemCommandMsg::SetModeAction::EnterDiagnosticMode:
            logInfo("Entering diagnostic mode...");
            mode_changed = configureDiagnostic();
            break;
        default:
            logInfo("Unimplemented mode for FlightProcessor: " + std::to_string(static_cast<int>(action)));
            return false;
    }
    logInfo("Mode change complete: " + std::string(mode_changed ? "SUCCESS" : "FAILURE"));
    if(mode_changed)
    {
        lm_mode_ = setModeActionToSystemMode(action);
    }
    return mode_changed;
}

bool FlightProcessor::handleSystemCommand(const SystemCommandMsg *command_msg)
{
    // Check the command type
    const SystemCommandMsg::CommandID cmd = static_cast<SystemCommandMsg::CommandID>(command_msg->getCommandid());
    // Check if this is a calibration command
    switch(cmd)
    {
        case SystemCommandMsg::CommandID::SetMode:
        {
            return handleChangeMode(command_msg);
        }
        case SystemCommandMsg::CommandID::Calibrate:
        {
            if(lm_mode_ != SystemMode_Type::CALIBRATION)
            {
                logWarning("Received calibration command while not in CALIBRATION mode");
                return false;
            }
            logInfo("Processing calibration");
            return processCalibrationCommand(command_msg);
        }
        case SystemCommandMsg::CommandID::Configure:
        {
            if(lm_mode_ != SystemMode_Type::CALIBRATION)
            {
                logWarning("Received configure command while not in CALIBRATION mode");
                return false;
            }
            logInfo("Processing configure command");
            return processConfigurationCommand(command_msg);
        }
        default:
            logWarning("Unknown or unimplemented SystemCommandMsg command ID: " + std::to_string(static_cast<int>(cmd)) + " for: " + name_);
            return false;
    }
}

bool FlightProcessor::configureStandby()
{
    logInfo("Configuring standby mode for: " + name_);
    // Close and cleanup all cameras
    for(uint32_t cam = 0; cam < static_cast<uint32_t>(LMCameras::NUM_CAMERAS); ++cam)
    {
        // Ensure camera is closed and idle
        if(camera_[cam] && camera_[cam]->isCameraOpen())
        {
            logInfo("Closing camera " + std::to_string(cam) + " for standby mode");
            camera_[cam]->closeCamera();
        }
        loadCameraSettings(camera_[cam]);
        frame_buffer_[cam]->clear();
    }
    // Clear frame buffer
    return true;
}

bool FlightProcessor::configureCalibration()
{
    // Configure all cameras for synchronized operation
    for(uint32_t cam = 0; cam < static_cast<uint32_t>(LMCameras::NUM_CAMERAS); ++cam)
    {
        loadCameraSettings(camera_[cam]);
        // Open the camera if not already open
        if(camera_[cam] == nullptr)
        {
            logError("Camera interface is null for camera " + std::to_string(cam));
            return false;
        }
        if(camera_[cam]->isCameraOpen())
        {
            logInfo("Camera " + std::to_string(cam) + " already open, closing for re-initialization");
            camera_[cam]->closeCamera();
        }
        if (!camera_[cam]->openCamera())
        {
            logError("Failed to open camera " + std::to_string(cam));
            return false;
        }
        if(!camera_[cam]->configureStream(libcamera::StreamRole::VideoRecording))
        {
            logError("Failed to configure camera stream for camera " + std::to_string(cam));
            return false;
        }
        if(!camera_[cam]->start())
        {
            logError("Failed to start camera " + std::to_string(cam));
            return false;
        }
        // Start continuous capture for each camera with a callback that includes the camera index
        // Using std::bind to pass the camera index to the callback so we can identify which camera captured each frame
        if(!camera_[cam]->startContinuousCapture(std::bind(&FlightProcessor::viewfinderCallback, this, std::placeholders::_1, cam)))
        {
            logError("Failed to start continuous capture for camera " + std::to_string(cam));
            return false;
        }
        // Instantiate distortion calibrator TODO: make this dynamic later, maybe instantiate on demand
        frame_counter_[cam] = 0; // Reset frame counter
    }
    distortion_calibrator_ = std::make_unique<CalibrateDistortion>(current_calibration_model_);
    if(distortion_calibrator_ == nullptr)
    {
        logError("Failed to create distortion calibrator for: " + name_);
        return false;
    }
    // Set checkerboard dimensions (inner corners) - TODO: make this dynamic later.
    // It could event be part of the calibration command parameters if we want to support different checkerboard patterns.
    distortion_calibrator_->setDimensions(6, 9); // default to 6x9 checkerboard
    return true;
}

bool FlightProcessor::configureLaunchMonitor()
{
    return true;
}

bool FlightProcessor::configureDiagnostic()
{
    return true;
}

void FlightProcessor::viewfinderCallback(cv::Mat &frame, const uint32_t camera_index)
{
    if(pause_stream_[camera_index].load())
    {
        return; // Skip streaming if paused
    }
    // Stream the captured frame with the camera index so we know which camera it came from
    // streamFrame(frame, camera_index, apply_calibrations_to_viewfinder_[camera_index].load());
    frame_counter_[camera_index]++;
}

bool FlightProcessor::processCalibrationCommand(const SystemCommandMsg *command)
{
    logInfo(name_ + ": Processing calibration command ID");

    // Parse calibration parameters from command
    const std::map<std::string, std::string> &commandParams = command->getCommandparams();
    const SystemCommandMsg::CalibrationAction act = static_cast<SystemCommandMsg::CalibrationAction>(command->getAction());

    int32_t camera_index = commandParams.find("camera_index") != commandParams.end() ? std::stoul(commandParams.at("camera_index")) : -1;
    if(camera_index < 0 || camera_index >= static_cast<int32_t>(LMCameras::NUM_CAMERAS))
    {
        logWarning(name_ + ": Invalid or missing camera index in calibration command, defaulting to camera 0");
        return false;
    }

    switch(act)
    {
        case SystemCommandMsg::CalibrationAction::SetBoardDimensions:
            // Handle SetBoardDimensions action
        {
            if (commandParams.find("rows") != commandParams.end() && commandParams.find("cols") != commandParams.end())
            {
                int rows = std::stoi(commandParams.at("rows"));
                int cols = std::stoi(commandParams.at("cols"));
                if(distortion_calibrator_)
                {
                    distortion_calibrator_->setDimensions(rows, cols);
                }
            }
            else
            {
                logWarning("Missing rows or cols parameter for SetBoardDimensions action");
                return false;
            }
        }
        break;
        case SystemCommandMsg::CalibrationAction::CaptureFrame:
            // Handle CaptureFrame action
        {
            cv::Mat cal_frame, debug_frame;
            frame_buffer_[camera_index]->getFrame(cal_frame);
            debug_frame = cal_frame.clone();
            if(distortion_calibrator_)
            {
                pause_stream_[camera_index].store(true);
                std::vector<cv::Point2f> corners;
                std::vector<cv::Point3f> object_points;
                const ImageQuality qual = distortion_calibrator_->processImage
                                          (
                    cal_frame,
                    debug_frame,
                    corners,
                    object_points
                                          );
                // streamFrame(debug_frame, camera_index, false);
                cal_utils_[camera_index].setLastCorners(corners);
                cal_utils_[camera_index].setLastObjectPoints(object_points);
            }
        }
        break;
        case SystemCommandMsg::CalibrationAction::AcceptFrame:
            // Handle AcceptFrame action
        {
            if(cal_utils_[camera_index].hasLastCorners() && cal_utils_[camera_index].hasLastObjectPoints())
            {
                cal_utils_[camera_index].addCalibrationData(
                    cal_utils_[camera_index].getLastCorners(),
                    cal_utils_[camera_index].getLastObjectPoints()
                    );
                cal_utils_[camera_index].clearLastCorners();
                cal_utils_[camera_index].clearLastObjectPoints();
            }
        }
        break;
        case SystemCommandMsg::CalibrationAction::RejectFrame:
            // Handle RejectFrame action
            break;
        case SystemCommandMsg::CalibrationAction::PerformDistortionCalibration:
            // Handle PerformDistortionCalibration action
            break;
        case SystemCommandMsg::CalibrationAction::PerformStereoCalibration:
            // Handle PerformStereoCalibration action
            break;
        case SystemCommandMsg::CalibrationAction::SelectModel:
        // Handle SelectModel action (unimplemented for now)
        default:
            logError(name_ + ": Unknown calibration action: " + std::to_string(static_cast<int>(act)));
            return false;
    }

    return true;
}

bool FlightProcessor::processConfigurationCommand(const SystemCommandMsg *command)
{
    bool config_change_success = true;
    const SystemCommandMsg::ConfigurationAction act = static_cast<SystemCommandMsg::ConfigurationAction>(command->getAction());
    const std::map<std::string, std::string> &commandParams = command->getCommandparams();
    // Process configuration commands. These can include things like changing
    // camera settings (exposure, gain, etc) or toggling whether to apply
    // calibrations to the viewfinder stream.
    const int32_t camera_index = commandParams.find("camera_index") != commandParams.end() ? std::stoul(commandParams.at("camera_index")) : -1; // Default to camera 0 if not specified
    if(camera_index < 0 || camera_index >= static_cast<int32_t>(LMCameras::NUM_CAMERAS))
    {
        logWarning(name_ + ": Invalid or missing camera index in configuration command, defaulting to camera 0");
        return false;
    }
    // Process specific configuration parameters for the camera
    return config_change_success;
}

void FlightProcessor::streamFrame(cv::Mat &frame, const uint32_t camera_index, const bool apply_calibration)
{
    // Validate frame before encoding
    if(frame.empty())
    {
        logWarning("Attempted to stream empty frame for camera " + std::to_string(camera_index));
        return;
    }
    // Add frame to buffer for potential use in calibration
    frame_buffer_[camera_index]->addFrame(frame);
    // Check if frame has valid channels for encoding (OpenCV imencode requirement)
    if (frame.channels() != 1 && frame.channels() != 3 && frame.channels() != 4)
    {
        logWarning("Attempted to stream frame with invalid channels (" + std::to_string(frame.channels()) + ") for camera " + std::to_string(camera_index));
        return;
    }
    // Check if codec is available
    if (!frame_codec_)
    {
        logWarning("Frame codec not available for camera " + std::to_string(camera_index));
        return;
    }
    // Apply distortion correction if valid calibration data is available
    // Use the camera_index parameter passed from the callback to determine which camera's calibration to use
    if(camera_[camera_index]->calibrationLoaded() && apply_calibration)
    {   // Only apply calibration if the flag is set, which allows us to stream
        // uncalibrated frames for testing or if the user prefers that way
        switch(cal_entry_[camera_index].calibration_type)
        { // Apply the appropriate distortion correction based on the
            // calibration model type
            case CalibrationModel::STANDARD:
                if (!CalUtils::undistortFrame(frame, camera_matrix_[camera_index].toMat(), dist_coeffs_[camera_index].toMat()))
                {
                    logWarning("Standard undistortion failed for camera " + std::to_string(camera_index));
                }
                break;
            case CalibrationModel::FISHEYE:
                if (!CalUtils::undistortFrameFisheye(frame, camera_matrix_[camera_index].toMat(), dist_coeffs_[camera_index].toMat(), camera_matrix_[camera_index].toMatScaled()))
                {
                    logWarning("Fisheye undistortion failed for camera " + std::to_string(camera_index));
                }
                break;
            default:
                break;
                // Maybe handle this case, but it shouldn't ever happen, and we
                // dont want to spam warnings
                // logWarning("Unknown calibration model type for camera " +
                // std::to_string(camera_index_) + " - skipping distortion
                // correction");
        }
    }
    // Encode the frame using the specified codec and parameters
    std::vector<uint8_t> encoded_data = frame_codec_->encode(frame, frame_codec_params_);
    // Check if encoding was successful
    if (encoded_data.empty())
    {
        logWarning("Failed to encode frame for camera " + std::to_string(camera_index) + " using codec: " + frame_codec_->getCodecName());
        return;
    }
    // Create a frame message and push it
    CameraFrameMsg frame_msg(
        "Camera_" + std::to_string(camera_index),
        frame_counter_[camera_index],
        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                  std::chrono::system_clock::now().time_since_epoch()).count()),
        camera_[camera_index]->getFrameRate(),
        encoded_data,
        { {"Codec", "JPEG"}
            , {"Quality", "90"}
            , {"ExposureTime", std::to_string((double)current_camera_settings_[camera_index].exposure_time_us * Constants::MICROSECONDS_TO_SECONDS)}
            , {"AnalogGain", std::to_string(current_camera_settings_[camera_index].analog_gain)}
            , {"FOVScale", std::to_string(current_camera_settings_[camera_index].fov_scale)}
        }
        );
    // data_publisher_->sendMessage(frame_msg);
}

void FlightProcessor::configureCamera(std::unique_ptr<GSCameraInterface> &camera, const CameraControlSettings_Type &settings)
{
    // This function can be called to apply new camera settings on the fly, for example in response to user configuration commands. For now, it only
    // supports changing exposure and gain, but it can be expanded in the future to support more settings as needed.
    if(camera)
    {
        camera->setAnalogGain(settings.analog_gain);
        camera->setExposureTime(settings.exposure_time_us);
        camera->setFOV(settings.fov_scale, settings.fov_scale);
    }
}

void FlightProcessor::loadCameraSettings(std::unique_ptr<GSCameraInterface> &camera)
{
    if(!calibration_data_)
    {
        calibration_data_ = CalibrationData::getInstance();
    }
    // Attempt to load existing calibration data for this camera
    const std::string uuid = camera->getUUIDInfo().uuid;
    CalibrationEntry_Type cal_entry;
    if(calibration_data_->getBestCalibrationEntry(uuid, cal_entry))
    {
        DistortionCoefficients_Type dist_coeffs;
        CameraIntrinsics_Type cam_intrinsics;
        bool success = false;
        switch(cal_entry.calibration_type)
        {
            case CalibrationModel::STANDARD:
                if(calibration_data_->getCalibrationEntryData(cal_entry, dist_coeffs.coeffs.standard, cam_intrinsics))
                {
                    camera->setCalibrationMatrix(cam_intrinsics.toMat());
                    camera->setDistortionCoefficients(CalibrationModel::STANDARD, dist_coeffs.toMat());
                }
                break;
            case CalibrationModel::FISHEYE:
                if(calibration_data_->getCalibrationEntryData(cal_entry, dist_coeffs.coeffs.fisheye, cam_intrinsics))
                {
                    camera->setCalibrationMatrix(cam_intrinsics.toMat());
                    camera->setDistortionCoefficients(CalibrationModel::FISHEYE, dist_coeffs.toMat());
                }
                break;
            default:
                logWarning("Unknown calibration model type in retrieved calibration entry for: " + name_);
        }
    }
    else
    {
        logInfo("No existing distortion calibration found for viewfinder stream for camera");
    }
    // Load the current camera settings from the database if they exist, so we can apply them to the camera when we start it.
    // This allows settings to persist across mode changes and restarts.
    // Set some default settings for now. Ideally, default values could be pulled from a
    // config file for the camera type or something like that later.
    CameraControlSettings_Type ctrl_settings;
    ctrl_settings.analog_gain = 4.0f;
    ctrl_settings.exposure_time_us = 20000; // 20ms
    ctrl_settings.fov_scale = 1.0f; // Default to no FOV scaling
    if(!calibration_data_->getCameraSettings(uuid, ctrl_settings))
    {
        logWarning("No existing camera settings found in database for UUID: " + uuid + " for: " + name_);
        // Save the defaults to the database so they can be loaded and applied next time
        if(!calibration_data_->setCameraSettings(uuid, ctrl_settings))
        {
            logError("Failed to store default camera settings in database for UUID: " + uuid + " for: " + name_);
        }
        else
        {
            logInfo("Default camera settings stored in database for UUID: " + uuid + " for: " + name_);
        }
    }
    logInfo("Current camera settings for " + name_ + " - Exposure time: " + std::to_string(ctrl_settings.exposure_time_us) + " us, Analog gain: " +
            std::to_string(ctrl_settings.analog_gain) + ", FOV scale: " + std::to_string(ctrl_settings.fov_scale));
    configureCamera(camera, ctrl_settings);
}

const bool FlightProcessor::loadCameraInfo(std::unique_ptr<GSCameraInterface> &camera)
{
    if(!calibration_data_)
    {
        calibration_data_ = CalibrationData::getInstance();
    }
    // Check if camera info exists in the calibration database, if so retrieve
    // the stored basic info, otherwise create a new entry
    GSCameraInterface::CameraUUIDInfo_Type camera_uuid_info_entry = camera->getUUIDInfo();
    CameraInfo_Type camera_info_entry;
    if(!calibration_data_->getCameraInfo(camera_uuid_info_entry.uuid, camera_info_entry))
    {
        logWarning("No existing camera info found in database for UUID: " + camera_uuid_info_entry.uuid + " for camera " + std::to_string(camera->getCameraIndex()) + ". Creating new entry.");
        // Populate camera_info_entry with basic info
        camera_info_entry.camera_id = camera->getCameraIndex();
        camera_info_entry.camera_name = name_ + "_Cam" + std::to_string(camera->getCameraIndex());
        camera_info_entry.camera_type = camera->getIDInfo().model;
        camera_info_entry.uuid = camera_uuid_info_entry.uuid;
        if(!calibration_data_->putCameraInfo(camera_info_entry))
        {
            logError("Failed to insert new camera info into database for UUID: " + camera_uuid_info_entry.uuid + " for camera " + std::to_string(camera->getCameraIndex()));
            return false;
        }
    }
    camera->storeInfo(camera_info_entry);
    return true;
}

void FlightProcessor::enableBallDetection(const bool enable)
{
    if(enable)
    {
        logInfo("Enabling ball detection for: " + name_);
        enable_ball_detection_.store(true);
    }
    else
    {
        logInfo("Disabling ball detection for: " + name_);
        enable_ball_detection_.store(false);
    }
}

void FlightProcessor::cleanUp()
{
    logInfo("Cleaning up: " + name_);
    // Stop current operations
    // Close all cameras if open to release resources and prepare for new mode
    for(uint32_t cam = 0; cam < static_cast<uint32_t>(LMCameras::NUM_CAMERAS); ++cam)
    {
        frame_buffer_[cam]->clear();
        frame_counter_[cam] = 0;
        if(camera_[cam])
        {
            try {
                if (camera_[cam]->isCameraOpen())
                {
                    // Stop any ongoing capture first
                    if (camera_[cam]->isCameraCapturing())
                    {
                        logInfo("Stopping continuous capture for camera " + std::to_string(cam) + ": " + name_);
                        camera_[cam]->stopContinuousCapture();
                    }
                    logInfo("Closing camera " + std::to_string(cam) + ": " + name_);
                    camera_[cam]->stop();
                    camera_[cam]->closeCamera();
                }
            } catch (const std::exception &e) {
                logError("Exception during camera " + std::to_string(cam) + " cleanup: " + std::string(e.what()));
            }
        }
        // Stream a black frame to indicate mode change or shutdown, but only if
        // primary camera (0) and codec are available
        if (camera_[cam] && frame_codec_ && camera_[cam]->isInitialized())
        {
            try {
                // Create a proper black frame with 3 channels (BGR) for JPEG
                // encoding
                cv::Mat black_frame = cv::Mat::zeros(camera_[cam]->getResolutionY(), camera_[cam]->getResolutionX(), CV_8UC3);
                // streamFrame(black_frame, cam, false); // Don't apply calibration to the black frame, camera cam
            } catch (const std::exception &e) {
                logWarning("Exception streaming black frame: " + std::string(e.what()));
            }
        }
    }

    logInfo("Cleanup completed: " + name_);
}

void FlightProcessor::cleanupProcess()
{
    TaskBase::cleanupProcess();
    // Call the internal cleanup function
    cleanUp();
    // Disconnect the frame publisher
    if(data_publisher_)
    {
        data_publisher_->disconnect(Endpoints::getDataCollectionEndpoint());
    }
}

void FlightProcessor::configureCameraConstants(std::unique_ptr<GSCameraInterface> &camera, const float focal_length, const float sensor_width, const float sensor_height)
{
    if(camera)
    {
        camera->setFocalLength(focal_length);
        camera->setSensorSize(sensor_width, sensor_height);
    }
}

void FlightProcessor::configureCameraStream(std::unique_ptr<GSCameraInterface> &camera, const uint32_t res_x, const uint32_t res_y, const float frame_rate, const TriggerMode trigger_mode)
{
    if(camera)
    {
        camera->setResolution(res_x, res_y);
        camera->setFrameRate(frame_rate);
        camera->setTriggerMode(trigger_mode);
    }
}
} // namespace PiTrac