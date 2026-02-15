#include "Application/Agents/CameraAgent/CameraAgent.h"
#include "Interfaces/Camera/GSCameraBase/GSCameraBase.h"
#include "Common/Utils/Calibration/CalibrationUtils.h"
#include <libcamera/camera_manager.h>
#include <thread>
#include <semaphore>
#include <future>

namespace PiTrac
{
CameraAgent::CameraAgent(const size_t camera_index, const std::string &process_name)
    : AgentBase(process_name)
    , frame_buffer_(std::make_shared<FrameBuffer>(4)) // Default buffer size of
                                                       // 4 frames
    , camera_(nullptr)
    , distortion_calibrator_(nullptr)
    , camera_index_(camera_index)
    , pause_stream_(false)
    , frame_counter_(0)
    , apply_calibrations_to_viewfinder_(false)
    , use_best_calibration_(true)
    , current_calibration_model_(CalibrationModel::FISHEYE) // Default to standard model, can be changed via config/command
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
    // Instantiate the camera manager and start it
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
    // Initialize the camera
    if(!camera_->initialize())
    {
        logError("Failed to initialize camera interface for: " + name_);
        return false;
    }
    // Load camera settings that will not change or are not configurable at runtime (like resolution) and apply.
    // Again these are currently hard-coded but should be loaded from config file in the future
    camera_->setResolution(1456, 1088); // Full resolution
    camera_->setSensorSize(3.674f, 2.760f); // IMX219 sensor size in mm
    camera_->setFocalLength(2.8f); // Focal length in mm (estimated for IMX219)
    camera_->setFrameRate(10.0f); // 10 FPS max for to reduce CPU load, can be increased if needed
    camera_->setTriggerMode(TriggerMode::FREE_RUNNING);
    // Load dynamic camera settings from the calibration database (like exposure time, gain, etc) and apply
    loadCameraSettings();
    // Retrieve & validate camera UUID and basic info
    camera_uuid_info_ = camera_->getUUIDInfo();
    if(!camera_uuid_info_.isValid())
    {
        logError("Invalid camera UUID info for: " + name_);
        return false;
    }
    camera_basic_info_ = camera_->getInfo();
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
            configureStandby();
            // Nothing to do, just ensure camera is closed and idle, and we are
            // ready to switch modes
            break;
        case SystemMode_Type::VIEWFINDER:
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
            if(lm_mode_ != SystemMode_Type::CALIBRATION && lm_mode_ != SystemMode_Type::VIEWFINDER)
            {
                logWarning("Received calibration command while not in CALIBRATION or VIEWFINDER mode for: " + name_);
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

bool CameraAgent::configureStandby()
{
    logInfo("Configuring standby mode for: " + name_);
    // NOTE: Loading the config database should really only be done once at startup,
    // however we want the system manager to be the first to access the database to ensure it
    // is loaded and ready before any agents attempt to access it, so for now we will load it
    // in each agent's standby mode configuration since all agents should start in standby mode. 
    // We can optimize this later by implementing a more robust database access layer with connection pooling and shared instances,
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
    // Check if camera info exists in the calibration database, if so retrieve
    // the stored basic info, otherwise create a new entry
    if(!calibration_data_->getCameraInfo(camera_uuid_info_.uuid, camera_info_))
    {
        logWarning("No existing camera info found in database for UUID: " + camera_uuid_info_.uuid + " for: " + name_ + ". Creating new entry.");
        // Populate camera_info_ with basic info
        camera_info_.camera_id = static_cast<uint32_t>(camera_index_);
        camera_info_.camera_name = name_ + "_Cam";
        camera_info_.camera_type = camera_basic_info_.model;
        camera_info_.uuid = camera_uuid_info_.uuid;
        if(!calibration_data_->putCameraInfo(camera_info_))
        {
            logError("Failed to insert new camera info into database for UUID: " + camera_uuid_info_.uuid + " for: " + name_);
            return false;
        }
    }
    // Ensure camera is closed and idle
    if(camera_->isCameraOpen())
    {
        logInfo("Closing camera for standby mode: " + name_);
        camera_->closeCamera();
    }
    loadCameraSettings();
    // Clear frame buffer
    frame_buffer_->clear();
    return true;
}

bool CameraAgent::configureViewfinder()
{
    logInfo("Configuring viewfinder mode for: " + name_);
    // TODO: Load these settings from a config file or parameters later
    loadCameraSettings();
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
    if(!camera_->startContinuousCapture(std::bind(&CameraAgent::viewfinderCallback, this, std::placeholders::_1)))
    {
        logError("Failed to start continuous capture for: " + name_);
        return false;
    }
    // Instantiate distortion calibrator TODO: make this dynamic later, maybe
    // instantiate on demand
    distortion_calibrator_ = std::make_unique<CheckerboardCalibration>();
    if(distortion_calibrator_ == nullptr)
    {
        logError("Failed to create distortion calibrator for: " + name_);
        return false;
    }
    // Set checkerboard dimensions (inner corners) - TODO: make this dynamic later. 
    // It could event be part of the calibration command parameters if we want to support different checkerboard patterns.
    distortion_calibrator_->setDimensions(6, 9); // default to 6x9 checkerboard
    distortion_calibrator_->setCalibrationModel(current_calibration_model_);
    frame_counter_ = 0; // Reset frame counter
    return true;
}

void CameraAgent::viewfinderCallback(cv::Mat &frame)
{
    frame_buffer_->addFrame(frame);
    if(pause_stream_)
    {
        return; // Skip streaming if paused
    }
    // Stream the captured frame.
    streamFrame(frame, apply_calibrations_to_viewfinder_);
    frame_counter_++;
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
            logInfo(name_ + ": Capturing calibration image");
            // Capture a single frame for calibration, use the frame buffer from the viewfinder stream to ensure we get the most recent frame and avoid interrupting the continuous capture for viewfinder mode. The viewfinder callback will continue to add frames to the buffer, so we just need to grab the latest one when we get this command.
            cv::Mat calibration_frame;
            if(!frame_buffer_->getFrame(calibration_frame))
            {
                logError(name_ + ": Failed to capture calibration image - empty frame returned");
                return false;
            }
            // Pause streaming while we process this command to avoid conflicts with the viewfinder callback adding new frames to the buffer while we are trying to retrieve the latest frame for calibration. We will unpause after we are done processing this command.
            pause_stream_.store(true);
            cv::Mat debug_image;
            const CalibrateDistortion::ImageQuality quality = distortion_calibrator_->processImage(calibration_frame, debug_image); 
            logInfo(name_ + ": Calibration image captured with quality: " + CalibrateDistortion::imageQualityToString(quality));
            // Stream the debug image without applying calibration so the user can see the quality metrics and decide whether to keep or discard this calibration image.
            streamFrame(debug_image, false); 
        }
        else if(action == "accept_image")
        {
            logInfo(name_ + ": Accepting calibration image and adding to calibrator");
            const size_t image_index = distortion_calibrator_->appendImage();
            pause_stream_.store(false); // Unpause streaming after processing the captured image
        }
        else if(action == "reject_image")
        {
            logInfo(name_ + ": Last calibration image rejected and cleared from buffer");
            distortion_calibrator_->clearLastImageData();
            pause_stream_.store(false); // Unpause streaming after rejecting the image
        }
        else if(action == "do_distortion_cal")
        {
            logInfo(name_ + ": Performing distortion calibration");
            // Retrieve all frames from the buffer for calibration
            distortion_calibrator_->setCalibrationModel(current_calibration_model_);
            pause_stream_.store(false);
            if(!distortion_calibrator_->doDistortionCalibration())
            {
                logError(name_ + ": Distortion calibration failed");
                return false; 
            }
            if(!distortion_calibrator_->isCalibrationValid())
            {
                logWarning(name_ + ": Distortion calibration completed but results may be invalid");
            }
            std::array<double, 4> intrinsics_coeffs = distortion_calibrator_->getCameraIntrinsics();
            CameraIntrinsics_Type intrinsics(intrinsics_coeffs);
            switch(current_calibration_model_)
            {
                case CalibrationModel::FISHEYE:
                {
                    std::array<double, 4> distortion_coeffs = distortion_calibrator_->getFisheyeDistortionCoefficients();
                    FisheyeDistortionCoefficients_Type distortionCoeffs(distortion_coeffs);
                    break;
                }
                case CalibrationModel::STANDARD:
                {
                    std::array<double, 5> distortion_coeffs = distortion_calibrator_->getDistortionCoefficients();
                    DistortionCoefficients_Type distortionCoeffs(distortion_coeffs);
                    break;
                }
            }
            valid_calibration_data_.store(true);
            logInfo(name_ + ": Distortion calibration completed successfully with reprojection error: " + std::to_string(distortion_calibrator_->getReprojectionError()) + " pixels");
        }
        else if(action == "save_calibration")
        {
            logInfo(name_ + ": Saving calibration results to database");
            // Save the latest calibration results to the database with a new entry
            CalibrationEntry_Type entryInfo;
            entryInfo.calibration_type = current_calibration_model_;
            entryInfo.reprojection_error = distortion_calibrator_->getReprojectionError();
            std::array<double, 4> intrinsics_coeffs = distortion_calibrator_->getCameraIntrinsics();
            CameraIntrinsics_Type intrinsics(intrinsics_coeffs);
            switch(current_calibration_model_)
            {
                case CalibrationModel::FISHEYE:
                {
                    logInfo(name_ + ": Fisheye calibration completed with reprojection error: " + std::to_string(entryInfo.reprojection_error) + " pixels");
                    std::array<double, 4> distortion_coeffs = distortion_calibrator_->getFisheyeDistortionCoefficients();
                    FisheyeDistortionCoefficients_Type distortionCoeffs(distortion_coeffs);
                    if(!calibration_data_->putCalibrationEntry(camera_uuid_info_.uuid, entryInfo, distortionCoeffs, intrinsics))
                    {
                        logError(name_ + ": Failed to store calibration results in database");
                        return false;
                    }
                    break;
                }
                case CalibrationModel::STANDARD:
                {
                    logInfo(name_ + ": Standard calibration completed with reprojection error: " + std::to_string(entryInfo.reprojection_error) + " pixels");
                    std::array<double, 5> distortion_coeffs = distortion_calibrator_->getDistortionCoefficients();
                    DistortionCoefficients_Type distortionCoeffs(distortion_coeffs);
                    if(!calibration_data_->putCalibrationEntry(camera_uuid_info_.uuid, entryInfo, distortionCoeffs, intrinsics))
                    {
                        logError(name_ + ": Failed to store calibration results in database");
                        return false;
                    }
                    break;
                }
            }
            logInfo(name_ + ": Calibration results stored in database successfully");
        }
        else if(action == "clear_buffer")
        {
            logInfo(name_ + ": Clearing calibration frame buffer");
            // Clear the frame buffer after calibration
            frame_buffer_->clear();
            distortion_calibrator_->clearImages();
        }
        else
        {
            logError(name_ + ": Unknown calibration action: " + action);
            return false;
        }
    }
    return true;
}

bool CameraAgent::processConfigurationCommand(const std::map<std::string, std::string> &commandParams)
{
    bool config_change_success = true;
    // Process configuration commands. These can include things like changing camera settings (exposure, gain, etc) or toggling whether to apply calibrations to the viewfinder stream.
    if(commandParams.find("apply_calibrations") != commandParams.end())
    {   // This will allow the user to toggle whether to apply calibrations to
        // the viewfinder stream on the fly,
        // which can be useful for testing calibration results or for users who
        // want the option to disable it
        // for performance reasons
        std::string apply_calibrations_str = commandParams.at("apply_calibrations");
        // Accept "true"/"1" as true, anything else as false
        apply_calibrations_to_viewfinder_ = (apply_calibrations_str == "true" || apply_calibrations_str == "1");
        logInfo(name_ + ": Setting apply_calibrations to " + std::to_string(apply_calibrations_to_viewfinder_));
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
            current_camera_settings_.exposure_time_us = static_cast<uint32_t>(exposure_s * 1e6); // Convert seconds to microseconds
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
            current_camera_settings_.analog_gain = gain;
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
            current_camera_settings_.fov_scale = fov_scale;
            logInfo(name_ + ": Setting FOV scale to " + std::to_string(fov_scale));
        }
        catch (const std::exception &e)
        {
            logError(name_ + ": Invalid FOV scale value: " + fov_scale_str + " - " + std::string(e.what()));
            config_change_success &= false;
        }
    }
    if(commandParams.find("apply_configuration") != commandParams.end())
    {   // This will apply any pending configuration changes that have been set via other commands (like exposure, gain, etc) on the fly without needing to switch modes
        // Apply the current camera settings to the camera interface immediately
        logInfo(name_ + ": Applying pending camera configuration changes");
        configureCamera(current_camera_settings_);
        // Check if we should also save the updated settings to the database
        std::string save_config_str = commandParams.at("apply_configuration");
        // Accept "true"/"1" as true, anything else as false
        bool save_config = (save_config_str == "true" || save_config_str == "1");
        if(save_config)
        {
            // Save the current camera settings to the database so they can be loaded on next startup or mode change
            if(!calibration_data_->setCameraSettings(camera_uuid_info_.uuid, current_camera_settings_))
            {
                logError(name_ + ": Failed to save camera settings to database");
                config_change_success &= false;
            }
            logInfo(name_ + ": Camera settings saved to database");
        }
        // If we are not saving to the database, the changes will still be applied for the current session, but they will not persist across restarts or mode changes
    }
    return config_change_success;
}

void CameraAgent::streamFrame(cv::Mat &frame, const bool apply_calibration)
{
    // Validate frame before encoding
    if(frame.empty())
    {
        logWarning("Attempted to stream empty frame for camera " + std::to_string(camera_index_));
        return;
    }
    // Check if frame has valid channels for encoding (OpenCV imencode
    // requirement)
    if (frame.channels() != 1 && frame.channels() != 3 && frame.channels() != 4)
    {
        logWarning("Attempted to stream frame with invalid channels (" + std::to_string(frame.channels()) + ") for camera " + std::to_string(camera_index_));
        return;
    }
    // Check if codec is available
    if (!frame_codec_)
    {
        logWarning("Frame codec not available for camera " + std::to_string(camera_index_));
        return;
    }
    // Apply distortion correction if valid calibration data is available
    if(valid_calibration_data_ && apply_calibration)
    {   // Only apply calibration if the flag is set, which allows us to stream
        // uncalibrated frames for testing or if the user prefers that way
        switch(cal_entry_.calibration_type)
        { // Apply the appropriate distortion correction based on the calibration model type
            case CalibrationModel::STANDARD:
                if (!CalUtils::undistortFrame(frame, camera_matrix_, dist_coeffs_mat_)) {
                    logWarning("Standard undistortion failed for " + name_);
                }
                break;
            case CalibrationModel::FISHEYE:
                if (!CalUtils::undistortFrameFisheye(frame, camera_matrix_, dist_coeffs_mat_, camera_matrix_scaled_)) {
                    logWarning("Fisheye undistortion failed for " + name_);
                }
                break;
            default:
                break;
                // Maybe handle this case, but it shouldn't ever happen, and we dont want to spam warnings
                // logWarning("Unknown calibration model type for camera " + std::to_string(camera_index_) + " - skipping distortion correction");
        }
    }
    // Encode the frame using the specified codec and parameters
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

void CameraAgent::configureCamera(const CameraControlSettings_Type &settings)
{
    // This function can be called to apply new camera settings on the fly, for
    // example in response to user configuration commands. For now, it only
    // supports changing exposure and gain, but it can be expanded in the future to support more settings as needed.
    camera_->setAnalogGain(settings.analog_gain);
    camera_->setExposureTime(settings.exposure_time_us);
    
    if(valid_calibration_data_)
    {
        // Reset scaled matrix from original before applying scale factor
        // This prevents cumulative scaling if configureCamera() is called multiple times
        camera_matrix_scaled_ = camera_matrix_.clone();
        
        // Note: We are only scaling the focal lengths here for FOV scaling, not the principal point, as that is 
        // a common approach for simulating a digital zoom effect. However, depending on the desired effect, we may 
        // also want to scale the principal point accordingly, especially if we begin to support configurable 
        // cropping or zooming in the future. For now, we will keep it simple and only scale the focal lengths.
        camera_matrix_scaled_.at<double>(0, 0) *= settings.fov_scale; // Scale fx
        camera_matrix_scaled_.at<double>(1, 1) *= settings.fov_scale; // Scale fy
    }
}

void CameraAgent::loadCameraSettings()
{
    if(!calibration_data_)
    {
        calibration_data_ = CalibrationData::getInstance();
    }
    // Attempt to load existing calibration data for this camera to apply to the
    // viewfinder stream
    if(calibration_data_->getBestCalibrationEntry(camera_uuid_info_.uuid, cal_entry_))
    {
        valid_calibration_data_ = true;
        switch(cal_entry_.calibration_type)
        {
            case CalibrationModel::STANDARD:
                if(!calibration_data_->getCalibrationEntryData(cal_entry_, dist_coeffs_, intrinsics_))
                {
                    logError("Failed to retrieve standard calibration distortion and intrinsics for: " + name_);
                    valid_calibration_data_ = false;
                }
                else
                {
                    dist_coeffs_mat_ = (cv::Mat_<double>(5, 1) << dist_coeffs_.k1, dist_coeffs_.k2, dist_coeffs_.p1, dist_coeffs_.p2, dist_coeffs_.k3);
                }
                break;
            case CalibrationModel::FISHEYE:
                if(!calibration_data_->getCalibrationEntryData(cal_entry_, fisheye_dist_coeffs_, intrinsics_))
                {
                    logError("Failed to retrieve fisheye calibration distortion and intrinsics for: " + name_);
                    valid_calibration_data_ = false;
                }
                else
                {
                    dist_coeffs_mat_ = (cv::Mat_<double>(4, 1) << fisheye_dist_coeffs_.k1, fisheye_dist_coeffs_.k2, fisheye_dist_coeffs_.k3, fisheye_dist_coeffs_.k4);
                }
                break;
            default:
                logWarning("Unknown calibration model type in retrieved calibration entry for: " + name_);
                valid_calibration_data_ = false;
        }
        
        camera_matrix_ = (cv::Mat_<double>(3, 3) << intrinsics_.focal_length_x, 0, intrinsics_.principal_point_x, 0, intrinsics_.focal_length_y, intrinsics_.principal_point_y, 0, 0, 1);
        // Scaled matrix will be configured in configureCamera() when we apply settings, since the scaling may depend on the current settings (e.g. FOV scale)
        // If not then we want it to default to the original camera matrix, so we initialize it here as a clone of the original camera matrix. 
        camera_matrix_scaled_ = camera_matrix_.clone();
        if(valid_calibration_data_)
        {            
            
            logInfo("Applying best distortion calibration to viewfinder stream for: " + name_);
            logInfo("Calibration entry ID: " + std::to_string(cal_entry_.calibration_id) + ", Type: " + std::to_string(static_cast<int>(cal_entry_.calibration_type)) + ", Reprojection Error: " + std::to_string(cal_entry_.reprojection_error) + " pixels, Date: " + cal_entry_.calibration_date);
            logInfo("Camera intrinsics - fx: " + std::to_string(intrinsics_.focal_length_x) + ", fy: " + std::to_string(intrinsics_.focal_length_y) + ", cx: " + std::to_string(intrinsics_.principal_point_x) + ", cy: " + std::to_string(intrinsics_.principal_point_y));
            if(cal_entry_.calibration_type == CalibrationModel::STANDARD)
            {
                logInfo("Distortion coefficients - k1: " + std::to_string(dist_coeffs_.k1) + ", k2: " + std::to_string(dist_coeffs_.k2) + ", p1: " + std::to_string(dist_coeffs_.p1) + ", p2: " + std::to_string(dist_coeffs_.p2) + ", k3: " + std::to_string(dist_coeffs_.k3));
            }
            else if(cal_entry_.calibration_type == CalibrationModel::FISHEYE)
            {
                logInfo("Fisheye distortion coefficients - k1: " + std::to_string(fisheye_dist_coeffs_.k1) + ", k2: " + std::to_string(fisheye_dist_coeffs_.k2) + ", k3: " + std::to_string(fisheye_dist_coeffs_.k3) + ", k4: " + std::to_string(fisheye_dist_coeffs_.k4));
            }
        }
    }
    else
    {
        logInfo("No existing distortion calibration found for viewfinder stream for: " + name_);
    }
    // Load the current camera settings from the database if they exist, so we can apply them to the camera when we start it. 
    // This allows settings to persist across mode changes and restarts.
    if(!calibration_data_->getCameraSettings(camera_uuid_info_.uuid, current_camera_settings_))
    {
        logWarning("No existing camera settings found in database for UUID: " + camera_uuid_info_.uuid + " for: " + name_);
        // Set some default settings for now. Ideally, we could pull this from a config file for the camera type or something like that later.
        current_camera_settings_.analog_gain = 4.0f;
        current_camera_settings_.exposure_time_us = 20000; // 20ms
        current_camera_settings_.fov_scale = 1.0f; // Default to no FOV scaling
        // Save the defaults to the database so they can be loaded and applied next time
        if(!calibration_data_->setCameraSettings(camera_uuid_info_.uuid, current_camera_settings_))
        {
            logError("Failed to store default camera settings in database for UUID: " + camera_uuid_info_.uuid + " for: " + name_);
        }
        else
        {
            logInfo("Default camera settings stored in database for UUID: " + camera_uuid_info_.uuid + " for: " + name_);
        }
    }
    logInfo("Current camera settings for " + name_ + " - Exposure time: " + std::to_string(current_camera_settings_.exposure_time_us) + " us, Analog gain: " + std::to_string(current_camera_settings_.analog_gain) + ", FOV scale: " + std::to_string(current_camera_settings_.fov_scale));
    configureCamera(current_camera_settings_);
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

    // Stream a black frame to indicate mode change or shutdown, but only if
    // camera and codec are available
    if (camera_ && frame_codec_ && camera_->isInitialized())
    {
        try {
            // Create a proper black frame with 3 channels (BGR) for JPEG
            // encoding
            cv::Mat black_frame = cv::Mat::zeros(camera_->getResolutionY(), camera_->getResolutionX(), CV_8UC3);
            streamFrame(black_frame, false); // Don't apply calibration to the
                                             // black frame
        } catch (const std::exception &e) {
            logWarning("Exception streaming black frame: " + std::string(e.what()));
        }
    }
    else
    {
        logInfo("Skipping black frame stream - camera or codec not available");
    }

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