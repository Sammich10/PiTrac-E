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
     * @param[in] width         Image width in pixels.
     * @param[in] height        Image height in pixels.
     * @param[in] focalLength   Focal length of the lens in millimeters.
     * @param[in] mode          Trigger mode for image acquisition (default:
     * FREE_RUNNING).
     */
    GSCameraBase(int width,
                 int height,
                 float focalLength,
                 TriggerMode mode = TriggerMode::FREE_RUNNING)
        : GSCameraInterface(width, height, focalLength, mode),
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
     * @brief Opens the camera for capturing.
     *
     * Initializes the camera and prepares it for capturing frames.
     *
     * @param[in] cameraIndex Index of the camera to open.
     * @return True if the camera was opened successfully, false otherwise.
     */
    bool openCamera(int cameraIndex) override;

    /**
     * @brief Initializes the camera after it has been opened.
     *
     * Configures the camera settings and prepares it for capturing frames.
     *
     * @return True if the camera was initialized successfully, false otherwise.
     */
    bool initializeCamera() override;

    /**
     * @brief Closes the camera.
     *
     * Stops capturing and releases the camera resources.
     */
    void closeCamera() override;

    /**
     * @brief Captures a single frame from the camera.
     *
     * @return The captured frame as a cv::Mat, or an empty Mat on failure.
     */
    cv::Mat captureFrame() override;

    /**
     * @brief Gets the next available frame from the camera.
     *
     * @return The next frame as a cv::Mat, or an empty Mat if no frame is
     * available.
     */
    cv::Mat getNextFrame() override;

    /**
     * @brief Gets the type of the camera.
     *
     * @return The camera type.
     */
    CAMERA_TYPE getCameraType() const override
    {
        return CAMERA_INNOMAKER_IMX296GS;
    }

    /**
     * @brief Sets the trigger mode for image acquisition.
     *
     * @param[in] mode The desired trigger mode (FREE_RUNNING or
     * EXTERNAL_TRIGGER).
     * @return True if the trigger mode was set successfully, false otherwise.
     */
    bool setTriggerMode(TriggerMode mode) override;

    /**
     * @brief Starts continuous image capture.
     *
     * In FREE_RUNNING mode, the camera captures frames continuously.
     * In EXTERNAL_TRIGGER mode, frames are captured upon receiving an external
     * trigger signal.
     *
     * @return True if continuous capture started successfully, false otherwise.
     */
    bool startContinuousCapture() override;

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
    size_t maxFrameBuffer_ = 100;

    /**
     * @brief Configures the camera with the required settings.
     *
     * @return true if the camera was successfully configured, false otherwise.
     */
    bool configureCamera() override;

    /**
     * @brief Allocates memory buffers required for the specified camera stream.
     *
     * @param stream Pointer to the libcamera::Stream object for which buffers
     * are to be allocated.
     *
     * @return true if buffer allocation was successful, false otherwise.
     */
    bool allocateBuffersForStream(libcamera::Stream *stream) override;

    /**
     * @brief Configures the camera to operate in trigger mode.
     *
     * @return true if the trigger mode was successfully configured; false
     * otherwise.
     */
    bool configureTriggerMode(const TriggerMode &mode) override;

    /**
     * @brief Converts a libcamera::FrameBuffer to an OpenCV cv::Mat object.
     *
     * @param buffer Pointer to the libcamera::FrameBuffer containing the image
     * data.
     * @return cv::Mat The resulting OpenCV matrix containing the image.
     */
    cv::Mat convertBufferToMat(libcamera::FrameBuffer *buffer) override;

    /**
     * @brief Handles the completion of a camera request.
     *
     * @param request Pointer to the completed libcamera::Request object.
     */
    void requestComplete(libcamera::Request *request) override;

    /**
     * @brief Adds a frame to the internal buffer.
     *
     * @param frame The image frame to be added to the buffer.
     */
    void addFrameToBuffer(const cv::Mat &frame) override;

    /**
     * @brief Switches the camera stream to the specified stream type.
     *
     * @param streamType The type of stream to switch to.
     *
     * @return true if the stream was successfully switched; false otherwise.
     */
    bool switchStream(StreamType streamType) override;
    /**
     * @brief Reconfigures the camera settings for an active streaming session.
     *
     * @return true if the reconfiguration was successful, false otherwise.
     */
    bool reconfigureForActiveStream();

    /**
     * @brief Get the latest captured frame
     *
     * In EXTERNAL_TRIGGER mode, this returns the most recent frame captured.
     * In FREE_RUNNING mode, this returns the latest frame in the continuous
     * stream.
     *
     * @return The latest frame as a cv::Mat, or an empty Mat if no frame is
     * available.
     */
    cv::Mat getLatestFrame();

    /**
     * @brief Get all available frames in the buffer (EXTERNAL_TRIGGER mode
     * only)
     *
     * Returns all frames currently stored in the internal buffer.
     * Clears the buffer after retrieval.
     *
     * @return A vector of cv::Mat containing all available frames.
     */
    std::vector<cv::Mat> getAllAvailableFrames();

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
     * @brief Clears all frames from the buffer (EXTERNAL_TRIGGER mode only)
     */
    void clearFrameBuffer();

    /**
     * @brief Unpacks 10-bit Bayer formatted image data into a cv::Mat object.
     *
     * @param data Pointer to the raw 10-bit Bayer image data.
     * @param width Width of the image in pixels.
     * @param height Height of the image in pixels.
     * @param stride Number of bytes per row in the input data.
     *
     * @return cv::Mat The unpacked image as an OpenCV matrix.
     */
    cv::Mat unpack10BitBayer(void *data, int width, int height, size_t stride);

    std::shared_ptr<GSLogger> logger_;

}; // class GSCameraBase
} // namespace PiTrac

#endif // IMX296_CAMERA_H