#ifndef GS_CAMERA_BASE_H
#define GS_CAMERA_BASE_H

#include "Interfaces/Camera/GSCameraInterface.h"
#include "Common/Utils/Logging/GSLogger.h"
namespace PiTrac
{
class GSCameraBase : public GSCameraInterface
{
  public:

    /**
     * @brief Constructs an GSCameraBase object with specified parameters.
     *
     * Initializes the camera interface for the IMX296 sensor, setting up sensor
     * dimensions,
     * field of view calculations, and trigger mode. The camera is not
     * configured or capturing
     * upon construction.
     *
     * @param[in] cameraIndex Index of the camera to be used.
     * @param[in] cameraManager Shared pointer to the libcamera::CameraManager
     * instance.
     *
     */
    GSCameraBase(const uint32_t &cameraIndex, std::shared_ptr<libcamera::CameraManager> const &cameraManager)
        : GSCameraInterface(cameraIndex, cameraManager),
        logger_(GSLogger::getInstance())
    {
    }

    /**
     * @brief Destructor for GSCameraBase.
     *
     * Cleans up resources, stops capturing if necessary, and releases the
     * camera.
     */
    ~GSCameraBase();

    /**
     * @brief Initializes the camera by acquiring it and retrieving camera
     *information.
     *
     * This method must be called before attempting to open the camera or
     *capture frames.
     *
     * @return true if initialization was successful, false otherwise.
     */
    bool initialize() override;

    /**
     * @brief Opens the camera for capturing.
     *
     * Attempts to acquire and open the camera at the specified index.
     *
     * @return True if the camera was opened successfully, false otherwise.
     */
    bool openCamera() override;

    /**
     * @brief Closes the camera.
     *
     * Stops capturing and releases the camera resources.
     */
    void closeCamera() override;

    /**
     * @brief Allocates memory buffers required for the specified camera stream,
     * and starts the camera.
     *
     * If successful, the camera will be ready to process capture requests.
     */
    bool start();

    /**
     * @brief Stops the camera from capturing.
     *
     * Halts frame capture, stops the camera, and releases allocated memory.
     *
     * @return True if the camera was stopped successfully, false otherwise.
     */
    bool stop();

    /**
     * @brief Captures a single frame from the camera.
     *
     * When running in continuous capture mode, this function returns the latest
     * frame captured.
     *
     * @return The captured frame as a cv::Mat, or an empty Mat on failure.
     */
    cv::Mat captureFrame
    (
        uint32_t timeout_ms = 2000
    ) override;

    /**
     * @brief Gets the type of the camera.
     *
     * @return The camera type.
     */
    CAMERA_TYPE getCameraType() const override
    {
        return CAMERA_GENERIC_RASPBERRY_PI;
    }

    /**
     * @brief Sets the trigger mode for image acquisition.
     *
     * @param[in] mode The desired trigger mode (FREE_RUNNING or
     * EXTERNAL_TRIGGER).
     * @return True if the trigger mode was set successfully, false otherwise.
     */
    bool setTriggerMode
    (
        TriggerMode mode
    ) override;

    /**
     * @brief Starts continuous image capture.
     *
     * In FREE_RUNNING mode, the camera captures frames continuously.
     * In EXTERNAL_TRIGGER mode, frames are captured upon receiving an external
     * trigger signal.
     *
     * @param[in] callback Optional callback function to handle each captured
     * frame,
     * used to process frames as they are captured in an event-driven manner. If
     * the
     * callback is nullptr, frames will be stored in an internal buffer for
     * later retrieval.
     *
     * @return True if continuous capture started successfully, false otherwise.
     */
    bool startContinuousCapture
    (
        requestCompleteCallback callback
    ) override;

    /**
     * @brief Stops continuous image capture.
     *
     * @return True if continuous capture stopped successfully, false otherwise.
     */
    bool stopContinuousCapture() override;

    /**
     * @brief Provides a string representation of the camera and its current
     * settings.
     *
     * @return A string describing the camera.
     */
    std::string toString() const override;

  private:

    // @brief Frame buffer for external trigger mode
    mutable std::mutex frameBufferMutex_;

    // @brief Buffer for storing captured frames.
    std::queue<cv::Mat> frameBuffer_;

    // @brief Maximum number of frames to buffer.
    size_t maxFrameBuffer_ = 10;

    // @brief Synchronization for single frame capture
    std::mutex singleFrameMutex_;
    std::condition_variable singleFrameCondition_;
    bool singleFrameReady_ = false;
    cv::Mat singleFrameResult_;

    /**
     * @brief Allocates memory buffers required for the specified camera stream.
     *
     * @param stream Pointer to the libcamera::Stream object for which buffers
     * are to be allocated.
     *
     * @return true if buffer allocation was successful, false otherwise.
     */
    bool allocateBuffersForStream
    (
        libcamera::Stream *stream
    ) override;

    /**
     * @brief Configures the camera to operate in trigger mode.
     *
     * @return true if the trigger mode was successfully configured; false
     * otherwise.
     */
    bool configureTriggerMode
    (
        const TriggerMode &mode
    ) override;

    /**
     * @brief Handles the completion of a camera request.
     *
     * @param request Pointer to the completed libcamera::Request object.
     */
    void requestComplete
    (
        libcamera::Request *request
    ) override;

    /**
     * @brief Adds a frame to the internal buffer.
     *
     * @param frame The image frame to be added to the buffer.
     */
    void addFrameToBuffer
    (
        const cv::Mat &frame
    ) override;

    /**
     * @brief Switches the camera stream to the specified stream type.
     *
     * @param streamType The type of stream to switch to.
     *
     * @return true if the stream was successfully switched; false otherwise.
     */
    bool configureStream
    (
        const libcamera::StreamRole &streamRole
    ) override;

    /**
     * @brief Reconfigures the camera settings for an active streaming session.
     *
     * @return true if the reconfiguration was successful, false otherwise.
     */
    bool reconfigureForActiveStream
    (
        const libcamera::StreamRole &streamRole
    );

    /**
     * @brief Allocates memory buffers for all active streams.
     *
     * @return true if buffer allocation was successful, false otherwise.
     */
    bool allocateBuffers();

    /**
     * @brief Frees all allocated memory buffers for all active streams.
     *
     * @return true if buffers were freed successfully, false otherwise.
     */
    bool freeBuffers();

    /**
     * @brief Creates libcamera::Request objects for capturing frames.
     *
     * @return true if requests were created successfully, false otherwise.
     */
    bool createRequests();

    /**
     * @brief Destroys all created libcamera::Request objects.
     *
     * @return true if requests were destroyed successfully, false otherwise.
     */
    bool destroyRequests();

    /**
     * @brief Check if there are frames available in the buffer
     *(EXTERNAL_TRIGGER mode only)
     *
     * @return True if there are frames available, false otherwise.
     */
    bool hasFramesAvailable() const;

    /**
     * @brief Get the number of frames currently in the buffer (EXTERNAL_TRIGGER
     * mode only)
     *
     * @return The number of frames in the buffer.
     */
    size_t getFrameQueueSize() const;

    /**
     * @brief Retrieves the latest frame from the buffer
     *
     * @return The latest captured frame as a cv::Mat. If no frames are
     * available, returns an empty Mat.
     */
    cv::Mat getLatestFrame();

    /**
     * @brief Clears all frames from the buffer (EXTERNAL_TRIGGER mode only)
     */
    void clearFrameBuffer();

    /**
     * @brief Set the maximum number of frames to buffer (EXTERNAL_TRIGGER mode
     * only)
     *
     * If the buffer exceeds this size, the oldest frames will be discarded.
     *
     * @param[in] maxFrames The maximum number of frames to buffer.
     */
    void setMaxFrameBuffer(size_t maxFrames)
    {
        maxFrameBuffer_ = maxFrames;
    }

    /**
     * @brief Get the maximum number of frames that can be buffered
     *(EXTERNAL_TRIGGER mode only)
     *
     * @return The maximum number of frames that can be buffered.
     */
    size_t getMaxFrameBuffer() const
    {
        return maxFrameBuffer_;
    }

    /**
     * @brief Retrieves camera information such as model, location, ID, and
     *sensor
     * details.
     */
    CameraI2CInfo getCameraI2CInfo
    (
        const std::string &deviceTreePath
    ) const;

    /**
     * @brief Retrieves camera information such as model, location, ID, and
     *sensor
     * details.
     */
    CameraInfo getCameraInfo() const;

    std::shared_ptr<GSLogger> logger_;
}; // class GSCameraBase
} // namespace PiTrac

#endif // IMX296_CAMERA_H