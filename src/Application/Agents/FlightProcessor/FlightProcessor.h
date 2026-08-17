#ifndef CAMERA_AGENT_H
#define CAMERA_AGENT_H

#include "Application/Agents/AgentBase/AgentBase.h"
#include "Infrastructure/DataStructures/FrameBuffer.h"
#include "Interfaces/Camera/GSCameraInterface.h"
#include "Common/Utils/CodecUtils/CodecUtils.h"
#include "Common/Utils/Calibration/CalibrationData.h"
#include "Common/Utils/Calibration/CalibrateDistortion.h"
#include "Common/Utils/Calibration/CalibrationStruct.h"
#include "Common/Utils/Detection/BallDetector.h"
#include "Common/Camera/CameraStructs.h"
#include "Common/System/System.h"
#include <opencv2/opencv.hpp>
#include <thread>
#include <atomic>
#include <memory>
#include <semaphore>
#include <queue>
#include <mutex>
#include "Infrastructure/Messaging/Messages/SystemCommandMsg.h"
#include "Infrastructure/Messaging/Messages/CameraConfigurationMsg.h"

namespace PiTrac
{
/**
 * @brief The FlightProcessor class is responsible for operating a camera
 * through the Camera Interface and publishing raw camera frames to a
 * messaging system.
 *
 * At it's core, it will implement different camera operations such as
 * opening, configuring, capturing frames, and closing the camera.
 * The FlightProcessor will also enqueue captured frames into a FrameBuffer
 * to be consumed by the FrameProcessorAgent.
 *
 * On top of this, it will support different modes of operation to support
 * the functionality of the launch monitor system.
 */
class FlightProcessor : public AgentBase
{
  public:
    /**
     * @brief Constructs a FlightProcessor object.
     *
     * @param camera_index Index of the camera
     */
    FlightProcessor
    (
        const std::string &process_name = "FlightProcessor"
    );

    /**
     * @brief Destructor for the FlightProcessor class.
     *
     * Cleans up resources and performs necessary shutdown procedures
     * when a FlightProcessor object is destroyed.
     */
    ~FlightProcessor();

    /**
     * @brief Sets up the camera agent.
     *
     * This method initializes the camera agent and prepares it for operation.
     *
     * @return true if setup was successful, false otherwise.
     */
    bool setupProcess() override;

    /**
     * @brief Cleans up resources used by the camera agent.
     *
     * This method releases any camera resources, and joins the capture thread.
     */
    void cleanupProcess() override;

  protected:

    bool changeMode
    (
        PiTrac::SystemMode_Type new_mode
    ) override;

    bool handleSystemCommand
    (
        const SystemCommandMsg &command_msg
    ) override;

    virtual bool configureStandby();

    /**
     * @brief Configures the camera for viewfinder mode.
     */
    virtual bool configureViewfinder();

    /**
     * @brief Starts the viewfinder mode operation in a separate thread.
     *
     * Viewfinder mode continuously captures frames from the camera and
     * publishes them to the messaging system.
     * @param frame The captured frame from the camera
     * @param camera_index The index of the camera that captured this frame
     */
    virtual void viewfinderCallback
    (
        cv::Mat &frame,
        const uint32_t camera_index
    );

    /**
     * @brief Processes a calibration command received from SystemManager.
     *
     * @param command The calibration command to process
     */
    virtual bool processCalibrationCommand
    (
        const std::map<std::string, std::string> &commandParams
    );

    virtual bool processConfigurationCommand
    (
        const std::map<std::string, std::string> &commandParams
    );

  private:

    /**
     * @brief Streams a frame to the messaging system.
     * @param frame The frame to stream
     */
    inline void streamFrame
    (
        cv::Mat &frame,
        const uint32_t camera_index,
        const bool apply_calibration = true
    );

    inline void configureCamera
    (
        const CameraControlSettings_Type &settings,
        const LMCameras camera_index
    );

    void enableBallDetection
    (
        const bool enable
    );

    void loadCameraSettings
    (
        const uint32_t camera_index
    );

    /**
     * @brief Cleans up resources used by the camera agent in
     * preparation for mode chang or shutdown.
     */
    inline void cleanUp();

    std::unique_ptr<MessagerBase> data_publisher_;
    std::unique_ptr<FrameCodec> frame_codec_;
    std::array<std::shared_ptr<FrameBuffer>, static_cast<size_t>(PiTrac::LMCameras::NUM_CAMERAS)> frame_buffer_;
    std::array<std::unique_ptr<GSCameraInterface>, static_cast<size_t>(PiTrac::LMCameras::NUM_CAMERAS)> camera_;
    std::unique_ptr<CalibrateDistortion> distortion_calibrator_;
    std::shared_ptr<CalibrationData> calibration_data_;
    std::unique_ptr<BallDetector> ball_detector_;
    std::array<std::atomic<bool>, static_cast<size_t>(PiTrac::LMCameras::NUM_CAMERAS)> pause_stream_;
    std::array<std::atomic<bool>, static_cast<size_t>(PiTrac::LMCameras::NUM_CAMERAS)> valid_calibration_data_;
    std::array<std::atomic<bool>, static_cast<size_t>(PiTrac::LMCameras::NUM_CAMERAS)> apply_calibrations_to_viewfinder_;
    std::atomic<bool> use_best_calibration_;
    std::atomic<bool> enable_ball_detection_;
    std::array<uint64_t, static_cast<size_t>(PiTrac::LMCameras::NUM_CAMERAS)> frame_counter_;
    CodecParams frame_codec_params_;
    // Calibration command handling
    std::mutex calibration_queue_mutex_;
    std::array<CameraInfo_Type, static_cast<size_t>(PiTrac::LMCameras::NUM_CAMERAS)> camera_info_;
    std::array<GSCameraInterface::CameraUUIDInfo, static_cast<size_t>(PiTrac::LMCameras::NUM_CAMERAS)> camera_uuid_info_;
    std::array<GSCameraInterface::CameraInfo, static_cast<size_t>(PiTrac::LMCameras::NUM_CAMERAS)> camera_basic_info_;
    CalibrationModel current_calibration_model_;
    std::array<CameraIntrinsics_Type, static_cast<size_t>(PiTrac::LMCameras::NUM_CAMERAS)> camera_matrix_;
    std::array<cv::Mat, static_cast<size_t>(PiTrac::LMCameras::NUM_CAMERAS)> dist_coeffs_mat_;
    std::array<CalibrationEntry_Type, static_cast<size_t>(PiTrac::LMCameras::NUM_CAMERAS)> cal_entry_;
    std::array<DistortionCoefficients_Type, static_cast<size_t>(PiTrac::LMCameras::NUM_CAMERAS)> dist_coeffs_;
    std::array<CameraIntrinsics_Type, static_cast<size_t>(PiTrac::LMCameras::NUM_CAMERAS)> intrinsics_;
    std::array<CameraControlSettings_Type, static_cast<size_t>(PiTrac::LMCameras::NUM_CAMERAS)> current_camera_settings_;
    std::array<std::vector<cv::Point2f>, static_cast<size_t>(PiTrac::LMCameras::NUM_CAMERAS)> last_corners_;
    std::array<std::vector<cv::Point3f>, static_cast<size_t>(PiTrac::LMCameras::NUM_CAMERAS)> last_object_points_;
    std::array<std::vector<std::vector<cv::Point2f> >, static_cast<size_t>(PiTrac::LMCameras::NUM_CAMERAS)> calibration_corners_buffer_;
    std::array<std::vector<std::vector<cv::Point3f> >, static_cast<size_t>(PiTrac::LMCameras::NUM_CAMERAS)> calibration_object_points_buffer_;
};
} // namespace PiTrac

#endif // CAMERA_AGENT_H