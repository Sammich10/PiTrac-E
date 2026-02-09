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
    , frame_buffer_(std::make_shared<FrameBuffer>(64)) // Default buffer size of
                                                       // 64 frames
    , camera_(nullptr)
    , distortion_calibrator_(nullptr)
    , camera_index_(camera_index)
    , running_(false)
    , frame_counter_(0)
    , apply_calibrations_to_viewfinder_(false)
    , use_best_calibration_(true)
    , current_calibration_model_(CalibrationModel::STANDARD) // Default to standard model, can be changed via config/command
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
            logInfo("Starting viewfinder mode for: " + name_);
            if(!configureViewfinder())
            {
                logError("Failed to configure viewfinder for: " + name_);
                cleanUp();
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
    // Clear frame buffer
    frame_buffer_->clear();
    return true;
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
        
        if(valid_calibration_data_)
        {
            // For now, hardcode the scaling since we know streaming is 544x728 and calibration was at full res
            // You can make this dynamic later by storing the calibration resolution in the database
            cv::Size streamingSize(728, 544);  // width x height of streaming frames
            cv::Size calibrationSize(1456, 1088); // width x height of calibration frames (estimated from cx value)
            
            // Calculate scale factors
            double scale_x = static_cast<double>(streamingSize.width) / static_cast<double>(calibrationSize.width);
            double scale_y = static_cast<double>(streamingSize.height) / static_cast<double>(calibrationSize.height);
            
            // Scale the camera matrix parameters
            double scaled_fx = intrinsics_.focal_length_x * scale_x;
            double scaled_fy = intrinsics_.focal_length_y * scale_y;
            double scaled_cx = intrinsics_.principal_point_x * scale_x;
            double scaled_cy = intrinsics_.principal_point_y * scale_y;
            
            camera_matrix_ = (cv::Mat_<double>(3, 3) << scaled_fx, 0, scaled_cx, 0, scaled_fy, scaled_cy, 0, 0, 1);
            
            logInfo("Applying best distortion calibration to viewfinder stream for: " + name_);
            logInfo("Calibration entry ID: " + std::to_string(cal_entry_.calibration_id) + ", Type: " + std::to_string(static_cast<int>(cal_entry_.calibration_type)) + ", Reprojection Error: " + std::to_string(cal_entry_.reprojection_error) + " pixels, Date: " + cal_entry_.calibration_date);
            logInfo("Original camera intrinsics (at " + std::to_string(calibrationSize.width) + "x" + std::to_string(calibrationSize.height) + ") - fx: " + std::to_string(intrinsics_.focal_length_x) + ", fy: " + std::to_string(intrinsics_.focal_length_y) + ", cx: " + std::to_string(intrinsics_.principal_point_x) + ", cy: " + std::to_string(intrinsics_.principal_point_y));        
            logInfo("Scaled camera intrinsics (for " + std::to_string(streamingSize.width) + "x" + std::to_string(streamingSize.height) + ") - fx: " + std::to_string(scaled_fx) + ", fy: " + std::to_string(scaled_fy) + ", cx: " + std::to_string(scaled_cx) + ", cy: " + std::to_string(scaled_cy));
            logInfo("Scale factors - x: " + std::to_string(scale_x) + ", y: " + std::to_string(scale_y));
            
            // Log the appropriate distortion coefficients based on calibration model
            if(cal_entry_.calibration_type == CalibrationModel::FISHEYE)
            {
                logInfo("Fisheye distortion coefficients (unchanged) - k1: " + std::to_string(fisheye_dist_coeffs_.k1) + ", k2: " + std::to_string(fisheye_dist_coeffs_.k2) + ", k3: " + std::to_string(fisheye_dist_coeffs_.k3) + ", k4: " + std::to_string(fisheye_dist_coeffs_.k4));
            }
            else
            {
                logInfo("Standard distortion coefficients (unchanged) - k1: " + std::to_string(dist_coeffs_.k1) + ", k2: " + std::to_string(dist_coeffs_.k2) + ", p1: " + std::to_string(dist_coeffs_.p1) + ", p2: " + std::to_string(dist_coeffs_.p2) + ", k3: " + std::to_string(dist_coeffs_.k3));
            }
        }
    }
    else
    {
        logInfo("No existing distortion calibration found for viewfinder stream for: " + name_);
    }
    if(!camera_->startContinuousCapture(std::bind(&CameraAgent::viewfinderCallback, this, std::placeholders::_1)))
    {
        logError("Failed to start continuous capture for: " + name_);
        return false;
    }
    frame_counter_ = 0; // Reset frame counter
    return true;
}

void CameraAgent::viewfinderCallback(cv::Mat &frame)
{
    // Stream the captured frame. No other functionality needed here for
    // viewfinder.
    streamFrame(frame, apply_calibrations_to_viewfinder_);
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
    // Instantiate distortion calibrator TODO: make this dynamic later, maybe
    // instantiate on demand
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
            logInfo(name_ + ": Capturing calibration image");
            cv::Mat calibration_frame = camera_->captureFrame(5000); // 5 second
                                                                     // timeout
            if (!calibration_frame.empty())
            {
                // Store in frame buffer for processing
                frame_buffer_->addFrame(calibration_frame);
                frame_counter_ = frame_buffer_->size();
                streamFrame(calibration_frame, false); // Stream captured
                                                       // calibration
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
            distortion_calibrator_->setCalibrationModel(current_calibration_model_);
            if(!distortion_calibrator_->doDistortionCalibration())
            {
                logError(name_ + ": Distortion calibration failed");
                return false;
            }
            if(!distortion_calibrator_->isCalibrationValid())
            {
                logWarning(name_ + ": Distortion calibration completed but results may be invalid");
            }
            // std::vector<cv::Mat> debug_images = distortion_calibrator_->getDebugImages();
            // Stream debug images if available
            // for(auto &dbg_img : debug_images)
            // {
            //     streamFrame(dbg_img, false); // Don't apply calibration to debug
            //                                  // images
            // }
            // Store calibration results in the database
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
            // Clear the frame buffer after calibration
            frame_buffer_->clear();
            distortion_calibrator_->clearImages();
            logInfo(name_ + ": Distortion calibration completed with " + std::to_string(calibration_frames.size()) + " frames");
            return true;
        }
        else if(action == "clear_buffer")
        {
            logInfo(name_ + ": Clearing calibration frame buffer");
            frame_buffer_->clear();
            distortion_calibrator_->clearImages();
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

bool CameraAgent::processConfigurationCommand(const std::map<std::string, std::string> &commandParams)
{
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
        return true;
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
        return true;
    }
    // Unknown or unhandled configuration command
    logWarning(name_ + ": Unknown configuration command action");
    return false;
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
                if (!CalUtils::undistortFrameFisheye(frame, camera_matrix_, dist_coeffs_mat_)) {
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