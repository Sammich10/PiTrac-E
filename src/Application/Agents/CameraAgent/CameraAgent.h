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
#include "Common/System/System.h"
#include <opencv2/opencv.hpp>
#include <thread>
#include <atomic>
#include <memory>
#include <semaphore>
#include <queue>
#include <mutex>
#include "Infrastructure/Messaging/Messages/SystemCommandMsg.h"

namespace PiTrac
{
/**
 * @brief The CameraAgent class is responsible for operating a camera
 * through the Camera Interface and publishing raw camera frames to a
 * messaging system.
 *
 * At it's core, it will implement different camera operations such as
 * opening, configuring, capturing frames, and closing the camera.
 * The CameraAgent will also enqueue captured frames into a FrameBuffer
 * to be consumed by the FrameProcessorAgent.
 *
 * On top of this, it will support different modes of operation to support
 * the functionality of the launch monitor system.
 */
class CameraAgent : public AgentBase
{
  public:
    /**
     * @brief Constructs a CameraAgent object.
     *
     * @param camera_index Index of the camera
     */
    CameraAgent
    (
        const size_t camera_index,
        const std::string &process_name = "CameraAgent"
    );

    /**
     * @brief Destructor for the CameraAgent class.
     *
     * Cleans up resources and performs necessary shutdown procedures
     * when a CameraAgent object is destroyed.
     */
    ~CameraAgent();

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
     */
    virtual void viewfinderCallback
    (
        cv::Mat &frame
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
        const bool apply_calibration = true
    );

    inline void configureCamera
    (
        const CameraControlSettings_Type &settings
    );

    void enableBallDetection
    (
        const bool enable
    );

    void loadCameraSettings();

    /**
     * @brief Cleans up resources used by the camera agent in
     * preparation for mode chang or shutdown.
     */
    inline void cleanUp();

    std::unique_ptr<MessagerBase> frame_publisher_;
    std::unique_ptr<FrameCodec> frame_codec_;
    std::shared_ptr<FrameBuffer> frame_buffer_;
    std::unique_ptr<GSCameraInterface> camera_;
    std::unique_ptr<CalibrateDistortion> distortion_calibrator_;
    std::shared_ptr<CalibrationData> calibration_data_;
    std::unique_ptr<BallDetector> ball_detector_;
    uint32_t camera_index_;
    std::atomic<bool> pause_stream_;
    std::atomic<bool> valid_calibration_data_;
    std::atomic<bool> apply_calibrations_to_viewfinder_;
    std::atomic<bool> use_best_calibration_;
    std::atomic<bool> enable_ball_detection_;
    uint64_t frame_counter_;
    CodecParams frame_codec_params_;
    // Calibration command handling
    std::mutex calibration_queue_mutex_;
    CameraInfo_Type camera_info_;
    GSCameraInterface::CameraUUIDInfo camera_uuid_info_;
    GSCameraInterface::CameraInfo camera_basic_info_;
    CalibrationModel current_calibration_model_;
    cv::Mat camera_matrix_;
    cv::Mat camera_matrix_scaled_;
    cv::Mat dist_coeffs_mat_;
    CalibrationEntry_Type cal_entry_;
    DistortionCoefficients_Type dist_coeffs_;
    FisheyeDistortionCoefficients_Type fisheye_dist_coeffs_;
    CameraIntrinsics_Type intrinsics_;
    CameraControlSettings_Type current_camera_settings_;
};
} // namespace PiTrac

#endif // CAMERA_AGENT_H