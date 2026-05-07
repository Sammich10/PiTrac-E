#include "Application/Agents/FlightProcessor/FlightProcessor.h"
#include "Interfaces/Camera/GSCameraBase/GSCameraBase.h"
#include "Common/Utils/Calibration/CalibrationUtils.h"
#include "Common/Utils/Detection/BallDetectionUtils.h"
#include "Common/Math/Constants.h"
#include <libcamera/camera_manager.h>
#include <thread>
#include <semaphore>
#include <future>

namespace PiTrac
{
FlightProcessor::FlightProcessor(const std::string &process_name)
    : AgentBase(process_name)
    , use_best_calibration_(true)
    , current_calibration_model_(CalibrationModel::FISHEYE)
{
    logInfo("FlightProcessor created: " + name_);
    for(uint32_t cam = 0; cam < static_cast<uint32_t>(LMCameras::NUM_CAMERAS); ++cam)
    {
        camera_[cam] = nullptr; // Initialize camera pointer to null
        frame_buffer_[cam] = std::make_unique<FrameBuffer>(4); // Initialize frame buffer pointer with default size
        pause_stream_[cam] = false; // Initialize pause stream flag
        valid_calibration_data_[cam] = false; // Initialize valid calibration data flag
        apply_calibrations_to_viewfinder_[cam] = false; // Initialize apply calibrations to viewfinder flag
        frame_counter_[cam] = 0; // Initialize frame counter
    }
    distortion_calibrator_ = nullptr; // Initialize distortion calibrator pointer to null
}

FlightProcessor::~FlightProcessor()
{
    logInfo("FlightProcessor destroyed: " + name_);
}

bool FlightProcessor::setupProcess()
{
    logInfo("Setting up " + name_);
    // Instantiate the camera manager and start it
    std::shared_ptr<libcamera::CameraManager> camera_manager = std::make_shared<libcamera::CameraManager>();
    const int ret = camera_manager->start();
    if (ret)
    {
        logError("Failed to start camera manager for: " + name_);
        return false;
    }
    // Instantiate the two cameras and initialize them
    for(uint32_t cam = 0; cam < static_cast<uint32_t>(LMCameras::NUM_CAMERAS); ++cam)
    {
        if(camera_manager->cameras().size() <= cam)
        {
            logError("Camera index " + std::to_string(cam) + " out of range for: " + name_);
            return false;
        }
        else
        {
            camera_[cam] = std::make_unique<GSCameraBase>(cam, camera_manager);
            // Instantiate the camera object
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
            // Load camera settings that will not change or are not configurable at runtime (like resolution) and apply.
            // Again these are currently hard-coded but should be loaded from config file in the future
            camera_[cam]->setResolution(1456, 1088); // Full resolution
            camera_[cam]->setSensorSize(3.674f, 2.760f); // IMX219 sensor size in mm
            camera_[cam]->setFocalLength(2.8f); // Focal length in mm (estimated for IMX219)
            camera_[cam]->setFrameRate(10.0f); // 10 FPS max for to reduce CPU load, can be
            // increased if needed
            camera_[cam]->setTriggerMode(TriggerMode::FREE_RUNNING);
            // Retrieve & validate camera UUID and basic info
            camera_uuid_info_[cam] = camera_[cam]->getUUIDInfo();
            if(!camera_uuid_info_[cam].isValid())
            {
                logError("Invalid camera UUID info for: " + name_);
                return false;
            }
            camera_basic_info_[cam] = camera_[cam]->getInfo();
        }
        // Load dynamic camera settings from the calibration database (like exposure time, gain, etc) and apply
        loadCameraSettings(cam);
    }
    // Instantiate the frame publisher
    data_publisher_ = std::make_unique<MessagerBase>(MessagerBase::SocketType::Push);
    if(data_publisher_ == nullptr)
    {
        logError("Failed to create frame publisher for: " + name_);
        return false;
    }
    try{
        // Create a PUSH messager to send frames to the frame collection endpoint
        data_publisher_->connect(Endpoints::getDataCollectionEndpoint());
    } catch (const std::exception &e) {
        logError("Exception connecting frame publisher for: " + name_ + " - " + std::string(e.what()));
        return false;
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
    return true;
}

bool FlightProcessor::changeMode(PiTrac::SystemMode_Type new_mode)
{
    cleanUp(); // Ensure previous mode is cleaned up
    switch(new_mode)
    {
        case SystemMode_Type::STANDBY:
            logInfo("Entering standby mode for: " + name_);
            configureStandby();
            // Nothing to do, just ensure camera is closed and idle, and we are ready to switch modes
            break;
        case SystemMode_Type::CALIBRATION:
            logInfo("Starting viewfinder/calibration mode for: " + name_);
            if(!configureViewfinder())
            {
                logError("Failed to configure viewfinder for: " + name_);
                cleanUp();
                return false;
            }
            return true;
        default:
            logInfo("Unimplemented mode for FlightProcessor: " + std::to_string(static_cast<int>(new_mode)));
            return false;
    }
    logInfo("Mode change complete to " + std::to_string(static_cast<int>(lm_mode_)) + " for: " + name_);
    return true;
}

bool FlightProcessor::handleSystemCommand(const SystemCommandMsg &command_msg)
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
        case SystemCommandMsg::CommandID::Configure:
        {
            logInfo("Processing configure command for: " + name_);
            return processConfigurationCommand(command_msg.getCommand_params());
        }
        default:
            logWarning("Unknown or unimplemented SystemCommandMsg command ID: " + std::to_string(static_cast<int>(cmd)) + " for: " + name_);
            return false;
    }
}

bool FlightProcessor::configureStandby()
{
    logInfo("Configuring standby mode for: " + name_);
    // NOTE: Loading the config database should really only be done once at startup, however we want the system manager to be the first to access the database
    // to ensure it is loaded and ready before any agents attempt to access it, so for now we will load it in each agent's standby mode configuration since all agents should start
    // in standby mode. We can optimize this later by implementing a more robust database access layer with connection pooling and shared instances,
    // or have the camera agent have a "first time initialization" flag to only load the database on the first transition to standby mode.
    if(!calibration_data_)
    {
        calibration_data_ = CalibrationData::getInstance();
    }
    if(calibration_data_ == nullptr)
    {
        logError("Failed to get CalibrationData instance for: " + name_);
        return false;
    }
    // Close and cleanup all cameras
    for(uint32_t cam = 0; cam < static_cast<uint32_t>(LMCameras::NUM_CAMERAS); ++cam)
    {
        // Check if camera info exists in the calibration database, if so retrieve
        // the stored basic info, otherwise create a new entry
        if(!calibration_data_->getCameraInfo(camera_uuid_info_[cam].uuid, camera_info_[cam]))
        {
            logWarning("No existing camera info found in database for UUID: " + camera_uuid_info_[cam].uuid + " for camera " + std::to_string(cam) + ". Creating new entry.");
            // Populate camera_info_ with basic info
            camera_info_[cam].camera_id = cam;
            camera_info_[cam].camera_name = name_ + "_Cam" + std::to_string(cam);
            camera_info_[cam].camera_type = camera_basic_info_[cam].model;
            camera_info_[cam].uuid = camera_uuid_info_[cam].uuid;
            if(!calibration_data_->putCameraInfo(camera_info_[cam]))
            {
                logError("Failed to insert new camera info into database for UUID: " + camera_uuid_info_[cam].uuid + " for camera " + std::to_string(cam));
                return false;
            }
        }
        // Ensure camera is closed and idle
        if(camera_[cam] && camera_[cam]->isCameraOpen())
        {
            logInfo("Closing camera " + std::to_string(cam) + " for standby mode");
            camera_[cam]->closeCamera();
        }
        loadCameraSettings(cam);
        frame_buffer_[cam]->clear();
    }
    // Clear frame buffer
    return true;
}

bool FlightProcessor::configureViewfinder()
{
    logInfo("Configuring viewfinder mode for: " + name_);
    // Configure all cameras for synchronized operation
    for(uint32_t cam = 0; cam < static_cast<uint32_t>(LMCameras::NUM_CAMERAS); ++cam)
    {
        loadCameraSettings(cam);
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

void FlightProcessor::viewfinderCallback(cv::Mat &frame, const uint32_t camera_index)
{
    if(pause_stream_[camera_index].load())
    {
        return; // Skip streaming if paused
    }
    // if(enable_ball_detection_)
    // {
    //     const bool ball_detected = ball_detector_->processFrame(frame);
    //     if(ball_detected)
    //     {
    //         ball_detector_->drawDebugInfo(frame, true); // Draw debug info for validated detections
    //     }
    //     else
    //     {
    //         cv::putText(frame, "No Ball Detected", cv::Point(10, 30),
    //                     cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 0, 255), 2);
    //     }
    // }
    // Stream the captured frame with the camera index so we know which camera it came from
    streamFrame(frame, camera_index, apply_calibrations_to_viewfinder_[camera_index].load());
    frame_counter_[camera_index]++;
}

bool FlightProcessor::processCalibrationCommand(const std::map<std::string, std::string> &commandParams)
{
    logInfo(name_ + ": Processing calibration command ID");
    // Parse calibration parameters from command
    if (commandParams.find("action") != commandParams.end())
    {
    std::string action = commandParams.at("action");
    int32_t camera_index = commandParams.find("camera_index") != commandParams.end() ? std::stoul(commandParams.at("camera_index")) : -1; // Default to camera 0 if not specified
    if(camera_index < 0 || camera_index >= static_cast<int32_t>(LMCameras::NUM_CAMERAS))
    {
        logWarning(name_ + ": Invalid or missing camera index in calibration command, defaulting to camera 0");
        return false;
    }
    if (action == "capture_image")
    {
        logInfo(name_ + ": Capturing calibration image");
        // Capture a single frame for calibration, use the frame buffer from the viewfinder stream to ensure we get the most recent frame and
        // avoid interrupting the continuous capture for viewfinder mode. The viewfinder callback will continue to add frames to the
        // buffer, so we just need to grab the latest one when we get this command.
        cv::Mat calibration_frame, debug_image;
        frame_buffer_[camera_index]->getFrame(calibration_frame);
        debug_image = calibration_frame.clone(); // Create a copy of the frame for drawing debug info
        const ImageQuality quality = distortion_calibrator_->processImage(calibration_frame, debug_image, last_corners_[camera_index], last_object_points_[camera_index]);
        // Stream the debug image without applying calibration so the user can see the quality metrics and decide whether to keep or discard this calibration image.
        pause_stream_[camera_index].store(true); // Pause streaming while we process this calibration image and wait for user feedback
        streamFrame(debug_image, camera_index, false);
        // logInfo(name_ + ": Calibration image captured with quality: " + ImageQualityToString(quality));
    }
    else if(action == "accept_image")
    {
        logInfo(name_ + ": Accepting calibration image and adding to calibrator");
        if(last_corners_[camera_index].empty() || last_object_points_[camera_index].empty())
        {
            logWarning(name_ + ": No valid corners or object points detected for the last calibration image, cannot accept image");
            return false;
        }
        calibration_corners_buffer_[camera_index].push_back(last_corners_[camera_index]);
        calibration_object_points_buffer_[camera_index].push_back(last_object_points_[camera_index]);
        last_corners_[camera_index].clear(); // Clear the last corners and object points after adding to the buffer
        last_object_points_[camera_index].clear();
        // Unpause streaming after processing the captured image
        pause_stream_[camera_index].store(false); 
    }
    else if(action == "reject_image")
    {
        logInfo(name_ + ": Last calibration image rejected and cleared from buffer");
        last_corners_[camera_index].clear();
        last_object_points_[camera_index].clear();
        pause_stream_[camera_index].store(false); // Unpause streaming after rejecting the image
    }
    else if(action == "do_distortion_cal")
    {
        logInfo(name_ + ": Performing distortion calibration");
        pause_stream_[camera_index].store(false);
        if(!distortion_calibrator_->doDistortionCalibration(calibration_corners_buffer_[camera_index], calibration_object_points_buffer_[camera_index]))
        {
            logError(name_ + ": Distortion calibration failed");
            return false;
        }
        camera_matrix_[camera_index] = distortion_calibrator_->getCameraMatrix();
        dist_coeffs_[camera_index] = distortion_calibrator_->getDistortionCoefficients();
        
        logInfo(name_ + ": Distortion calibration completed successfully with reprojection error: " + std::to_string(distortion_calibrator_->getReprojectionError()) + " pixels");
    }
    else if(action == "save_calibration")
    {
        if(!valid_calibration_data_[camera_index].load())
        {
            logWarning("No valid calibration data to save for camera index " + std::to_string(camera_index));
            return false;
        }
        logInfo(name_ + ": Saving calibration results to database");
        // Save the latest calibration results to the database with a new entry
        CalibrationEntry_Type entryInfo;
        entryInfo.calibration_type = distortion_calibrator_->getCalibrationModel();
        entryInfo.reprojection_error = distortion_calibrator_->getReprojectionError();
        bool save_success = false;
        switch(distortion_calibrator_->getCalibrationModel())
        {
            case CalibrationModel::STANDARD:
                save_success = calibration_data_->putCalibrationEntry(camera_info_[camera_index].uuid, entryInfo, dist_coeffs_[camera_index].coeffs.standard, camera_matrix_[camera_index]);
                break;
            case CalibrationModel::FISHEYE:
                save_success = calibration_data_->putCalibrationEntry(camera_info_[camera_index].uuid, entryInfo, dist_coeffs_[camera_index].coeffs.fisheye, camera_matrix_[camera_index]);
                break;
            default:
                logError(name_ + ": Unknown calibration model, cannot save to database");
                return false;
        }
        if(!save_success)
        {
            logError(name_ + ": Failed to save calibration results to database");
            return false;
        }
        logInfo("Calibration results stored in database successfully");
    }
    else if(action == "clear_buffer")
    {
        logInfo(name_ + ": Clearing calibration frame buffer");
        // Clear the frame buffer after calibration
        frame_buffer_[camera_index]->clear();
        distortion_calibrator_->reset();
        last_corners_[camera_index].clear();
        last_object_points_[camera_index].clear();
        calibration_corners_buffer_[camera_index].clear();
        calibration_object_points_buffer_[camera_index].clear();
    }
    else
    {
        logError(name_ + ": Unknown calibration action: " + action);
        return false;
    }
    }
    return true;
}

bool FlightProcessor::processConfigurationCommand(const std::map<std::string, std::string> &commandParams)
{
    bool config_change_success = true;
    // Process configuration commands. These can include things like changing
    // camera settings (exposure, gain, etc) or toggling whether to apply
    // calibrations to the viewfinder stream.
    const int32_t camera_index = commandParams.find("camera_index") != commandParams.end() ? std::stoul(commandParams.at("camera_index")) : -1; // Default to camera 0 if not specified
    if(camera_index < 0 || camera_index >= static_cast<int32_t>(LMCameras::NUM_CAMERAS))
    {
        logWarning(name_ + ": Invalid or missing camera index in configuration command, defaulting to camera 0");
        return false;
    }
    if(commandParams.find("apply_calibrations") != commandParams.end())
    {   // This will allow the user to toggle whether to apply calibrations to the viewfinder stream on the fly, which can be useful for testing calibration results or for users who  want the option to disable it for performance reasons
        std::string apply_calibrations_str = commandParams.at("apply_calibrations");
        // Accept "true"/"1" as true, anything else as false
        apply_calibrations_to_viewfinder_[camera_index] = (apply_calibrations_str == "true" || apply_calibrations_str == "1");
        logInfo(name_ + ": Setting apply_calibrations to " + std::to_string(apply_calibrations_to_viewfinder_[camera_index]));
    }
    if(commandParams.find("use_best_calibration") != commandParams.end())
    {   // This will allow the user to toggle what heuristic to use for loading
        // calibration data from the database.
        // If true, it will attempt to load the best available calibration for
        // this camera and apply it to the viewfinder stream.
        // If false, it default to the latest calibration data
        std::string use_best_calibration_str = commandParams.at("use_best_calibration");
        // Accept "true"/"1" as true, anything else as false
        use_best_calibration_ = (use_best_calibration_str == "true" || use_best_calibration_str == "1");
        // To avoid issues with loading calibrations while in viewfinder mode,
        // the new setting will be applied
        // on the next mode change to viewfinder, rather than immediately
    }
    if(commandParams.find("set_exposure") != commandParams.end())
    {
        std::string exposure_str = commandParams.at("set_exposure");
        try
        {
            double exposure_s = std::stod(exposure_str);
            // TODO: Determine which camera to apply settings to - for now apply to all
            for(uint32_t cam = 0; cam < static_cast<uint32_t>(LMCameras::NUM_CAMERAS); ++cam)
            {
                current_camera_settings_[cam].exposure_time_us = static_cast<uint32_t>(exposure_s * 1e6); // Convert seconds to microseconds
            }
            logInfo(name_ + ": Setting exposure time to " + std::to_string(exposure_s) + " seconds");
        }
        catch (const std::exception &e)
        {
            logError(name_ + ": Invalid exposure value: " + exposure_str + " - " + std::string(e.what()));
            config_change_success &= false;
        }
    }
    if(commandParams.find("set_gain") != commandParams.end())
    {
        std::string gain_str = commandParams.at("set_gain");
        try
        {
            float gain = std::stof(gain_str);
            // TODO: Determine which camera to apply settings to - for now apply to all
            for(uint32_t cam = 0; cam < static_cast<uint32_t>(LMCameras::NUM_CAMERAS); ++cam)
            {
                current_camera_settings_[cam].analog_gain = gain;
            }
            logInfo(name_ + ": Setting analog gain to " + std::to_string(gain));
        }
        catch (const std::exception &e)
        {
            logError(name_ + ": Invalid gain value: " + gain_str + " - " + std::string(e.what()));
            config_change_success &= false;
        }
    }
    if(commandParams.find("set_fov_scale") != commandParams.end())
    {
        std::string fov_scale_str = commandParams.at("set_fov_scale");
        try
        {
            float fov_scale = std::stof(fov_scale_str);
            // TODO: Determine which camera to apply settings to - for now apply to all
            for(uint32_t cam = 0; cam < static_cast<uint32_t>(LMCameras::NUM_CAMERAS); ++cam)
            {
                current_camera_settings_[cam].fov_scale = fov_scale;
            }
            logInfo(name_ + ": Setting FOV scale to " + std::to_string(fov_scale));
        }
        catch (const std::exception &e)
        {
            logError(name_ + ": Invalid FOV scale value: " + fov_scale_str + " - " + std::string(e.what()));
            config_change_success &= false;
        }
    }
    if(commandParams.find("apply_configuration") != commandParams.end())
    {   // This will apply any pending configuration changes that have been set
        // via other commands (like exposure, gain, etc) on the fly without
        // needing to switch modes
        // Apply the current camera settings to the camera interface immediately
        logInfo(name_ + ": Applying pending camera configuration changes");
        configureCamera(current_camera_settings_[camera_index], static_cast<LMCameras>(camera_index));
        // Check if we should also save the updated settings to the database
        std::string save_config_str = commandParams.at("apply_configuration");
        // Accept "true"/"1" as true, anything else as false
        bool save_config = (save_config_str == "true" || save_config_str == "1");
        if(save_config)
        {
            // Save the current camera settings to the database so they can be // loaded on next startup or mode change
            if(!calibration_data_->setCameraSettings(camera_uuid_info_[camera_index].uuid, current_camera_settings_[camera_index]))
            {
                logError(name_ + ": Failed to save camera " + std::to_string(camera_index) + " settings to database");
                config_change_success &= false;
            }
            logInfo(name_ + ": Camera settings saved to database");
        }
        // If we are not saving to the database, the changes will still be
        // applied for the current session, but they will not persist across
        // restarts or mode changes
    }
    // if(commandParams.find("enable_live_detection") != commandParams.end())
    // {
    //     std::string enable_str = commandParams.at("enable_live_detection");
    //     bool enable = (enable_str == "true" || enable_str == "1");
    //     enableBallDetection(enable);
    //     logInfo(name_ + ": Ball detection " + std::string(enable ? "enabled" : "disabled"));
    // }
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
    if(valid_calibration_data_[camera_index] && apply_calibration)
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
    data_publisher_->sendMessage(frame_msg);
}

void FlightProcessor::configureCamera(const CameraControlSettings_Type &settings, const LMCameras camera_index)
{
    // This function can be called to apply new camera settings on the fly, for example in response to user configuration commands. For now, it only
    // supports changing exposure and gain, but it can be expanded in the future to support more settings as needed.
    const uint32_t cam = static_cast<uint32_t>(camera_index);
    if(camera_[cam])
    {
        camera_[cam]->setAnalogGain(settings.analog_gain);
        camera_[cam]->setExposureTime(settings.exposure_time_us);
    }
    if(valid_calibration_data_[cam])
    {
        camera_matrix_[cam].scaleF(settings.fov_scale, settings.fov_scale);
    }
}

void FlightProcessor::loadCameraSettings(const uint32_t camera_index)
{
    if(!calibration_data_)
    {
        calibration_data_ = CalibrationData::getInstance();
    }
    // Attempt to load existing calibration data for this camera to apply to the
    // viewfinder stream
    if(calibration_data_->getBestCalibrationEntry(camera_uuid_info_[camera_index].uuid, cal_entry_[camera_index]))
    {
        valid_calibration_data_[camera_index] = true;
        switch(cal_entry_[camera_index].calibration_type)
        {
            case CalibrationModel::STANDARD:
                valid_calibration_data_[camera_index] = calibration_data_->getCalibrationEntryData(cal_entry_[camera_index], dist_coeffs_[camera_index].coeffs.standard, camera_matrix_[camera_index]);
                break;
            case CalibrationModel::FISHEYE:
                valid_calibration_data_[camera_index] = calibration_data_->getCalibrationEntryData(cal_entry_[camera_index], dist_coeffs_[camera_index].coeffs.fisheye, camera_matrix_[camera_index]);
                break;
            default:
                logWarning("Unknown calibration model type in retrieved calibration entry for: " + name_);
                valid_calibration_data_[camera_index] = false;
        }
        if(valid_calibration_data_[camera_index])
        {
            logInfo("Applying best distortion calibration to viewfinder stream");
        }
    }
    else
    {
        logInfo("No existing distortion calibration found for viewfinder stream for: " + name_);
    }
    // Load the current camera settings from the database if they exist, so we can apply them to the camera when we start it.
    // This allows settings to persist across mode changes and restarts.
    if(!calibration_data_->getCameraSettings(camera_uuid_info_[camera_index].uuid, current_camera_settings_[camera_index]))
    {
        logWarning("No existing camera settings found in database for UUID: " + camera_uuid_info_[camera_index].uuid + " for: " + name_);
        // Set some default settings for now. Ideally, we could pull this from a
        // config file for the camera type or something like that later.
        current_camera_settings_[camera_index].analog_gain = 4.0f;
        current_camera_settings_[camera_index].exposure_time_us = 20000; // 20ms
        current_camera_settings_[camera_index].fov_scale = 1.0f; // Default to no FOV scaling
        // Save the defaults to the database so they can be loaded and applied
        // next time
        if(!calibration_data_->setCameraSettings(camera_uuid_info_[camera_index].uuid, current_camera_settings_[camera_index]))
        {
            logError("Failed to store default camera settings in database for UUID: " + camera_uuid_info_[camera_index].uuid + " for: " + name_);
        }
        else
        {
            logInfo("Default camera settings stored in database for UUID: " + camera_uuid_info_[camera_index].uuid + " for: " + name_);
        }
    }
    logInfo("Current camera settings for " + name_ + " - Exposure time: " + std::to_string(current_camera_settings_[camera_index].exposure_time_us) + " us, Analog gain: " +
            std::to_string(current_camera_settings_[camera_index].analog_gain) + ", FOV scale: " + std::to_string(current_camera_settings_[camera_index].fov_scale));
    configureCamera(current_camera_settings_[camera_index], static_cast<LMCameras>(camera_index));
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
    run_.store(false);
    // Wait for current thread to finish
    if(agent_thread_.joinable())
    {
        agent_thread_.join();
    }
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
                streamFrame(black_frame, cam, false); // Don't apply calibration to the black frame, camera cam
            } catch (const std::exception &e) {
                logWarning("Exception streaming black frame: " + std::string(e.what()));
            }
        }
    }

    logInfo("Cleanup completed: " + name_);
}

void FlightProcessor::cleanupProcess()
{
    // Call the internal cleanup function
    cleanUp();
    // Disconnect the frame publisher
    if(data_publisher_)
    {
        data_publisher_->disconnect(Endpoints::getDataCollectionEndpoint());
    }
}
} // namespace PiTrac