#ifndef GS_CAMERA_INTERFACE_H
#define GS_CAMERA_INTERFACE_H

#include <opencv2/opencv.hpp>
#include <libcamera/libcamera.h>
#include <libcamera/camera_manager.h>
#include <libcamera/framebuffer_allocator.h>
#include <sys/mman.h>
#include <memory>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <memory>

namespace PiTrac
{
enum CAMERA_TYPE
{
    CAMERA_TYPE_UNKNOWN = 0,
    CAMERA_PICAM_V3,
    CAMERA_INNOMAKER_IMX296GS,
    CAMERA_TYPE_MAX
};

enum class TriggerMode
{
    FREE_RUNNING = 0,   // Normal continuous capture
    EXTERNAL_TRIGGER    // Wait for external trigger signal
};

enum class CameraStatus
{
    CAMERA_STATUS_OK = 0,
    CAMERA_STATUS_ERROR,
    CAMERA_STATUS_NOT_CONFIGURED,
    CAMERA_STATUS_NOT_OPEN,
    CAMERA_STATUS_MAX
};

enum class StreamType
{
    STREAM_TYPE_PREVIEW = 0,
    STREAM_TYPE_MAIN,
    STREAM_TYPE_HQ,
    STREAM_TYPE_MAX
};


class GSCameraInterface
{
/**
 * @class GSCameraInterface
 * @brief Abstract base class for camera interfaces.
 *
 * Provides a common interface for camera operations, calibration, and
 * properties.
 * Derived classes must implement core camera functionality. All derived camera
 * classes
 * are expected to utilize the libcamera library for camera management and frame
 * capture.
 *
 */
  public:

// You can also provide a protected constructor with common parameters
    GSCameraInterface(const uint32_t &cameraIndex, std::shared_ptr<libcamera::CameraManager> const &cameraManager)
        : cameraIndex_(cameraIndex)
        , resolutionX_(0)
        , resolutionY_(0)
        , focalLength_mm_(0)
        , cameraManager_(cameraManager)
        , camera_(nullptr)
        , allocator_(nullptr)
        , isConfigured_(false)
        , triggerMode_(TriggerMode::FREE_RUNNING)
        , isCapturing_(false)
    {
    }

    virtual ~GSCameraInterface() = default;

    /** Pure virtual methods to be implemented by derived classes **/
    virtual bool openCamera() = 0;
    virtual bool configureStream
    (
        const libcamera::StreamRole &streamRole
    ) = 0;
    virtual void closeCamera() = 0;
    virtual cv::Mat captureFrame() = 0;
    virtual cv::Mat getNextFrame() = 0;
    virtual CAMERA_TYPE getCameraType() const = 0;
    virtual bool setTriggerMode
    (
        TriggerMode mode
    ) = 0;
    virtual bool startContinuousCapture() = 0;
    virtual bool stopContinuousCapture() = 0;
    virtual std::string toString() const = 0;

    /** Accessor methods **/
    uint32_t getCameraIndex() const
    {
        return cameraIndex_;
    }

    int getResolutionX() const
    {
        return resolutionX_;
    }

    int getResolutionY() const
    {
        return resolutionY_;
    }

    float getFocalLength() const
    {
        return focalLength_mm_;
    }

    float getSensorWidth() const
    {
        return sensorWidth_mm_;
    }

    float getSensorHeight() const
    {
        return sensorHeight_mm_;
    }

    float getHorizontalFOV() const
    {
        return horizontalFOV_deg_;
    }

    float getVerticalFOV() const
    {
        return verticalFOV_deg_;
    }

    uint32_t getExposureTime() const
    {
        return currentExposureUs_;
    }

    float getAnalogGain() const
    {
        return currentGain_;
    }

    float getFrameRate() const
    {
        return currentFps_;
    }

    TriggerMode getTriggerMode() const
    {
        return triggerMode_;
    }

    cv::Mat getCalibrationMatrix() const
    {
        return calibrationMatrix_;
    }

    cv::Mat getDistortionCoefficients() const
    {
        return distortionCoefficients_;
    }

    bool isCameraOpen() const
    {
        return isCameraOpen_;
    }

    bool isCameraConfigured() const
    {
        return isConfigured_;
    }

    bool isCameraCapturing() const
    {
        return isCapturing_;
    }

    bool isUsingCalibrationMatrix() const
    {
        return useCalibrationMatrix_;
    }

    /** Mutator methods **/
    void setResolution
    (
        int resX, int resY
    )
    {
        resolutionX_ = resX; resolutionY_ = resY;
    }

    void setFocalLength
    (
        float focalLength
    )
    {
        focalLength_mm_ = focalLength;
    }

    void setSensorSize
    (
        float width, float height
    )
    {
        sensorWidth_mm_ = width; sensorHeight_mm_ = height;
    }

    void setFOV
    (
        float hFOV, float vFOV
    )
    {
        horizontalFOV_deg_ = hFOV; verticalFOV_deg_ = vFOV;
    }

    bool setExposureTime
    (
        uint32_t exposureUs
    )
    {
        currentExposureUs_ = exposureUs; return true;
    }

    bool setAnalogGain
    (
        float gain
    )
    {
        currentGain_ = gain; return true;
    }

    bool setFrameRate
    (
        float fps
    )
    {
        currentFps_ = fps; return true;
    }

    void setCalibrationMatrix
    (
        const cv::Mat &calibMatrix
    )
    {
        calibrationMatrix_ = calibMatrix;
    }

    void setDistortionCoefficients
    (
        const cv::Mat &distCoeffs
    )
    {
        distortionCoefficients_ = distCoeffs;
    }

    void setUseCalibrationMatrix
    (
        bool useCalib
    )
    {
        useCalibrationMatrix_ = useCalib;
    }

    void setResolutionOverride
    (
        int resX, int resY
    )
    {
        resolutionX_override_ = resX; resolutionY_override_ = resY;
    }

    void clearResolutionOverride
    (
    )
    {
        resolutionX_override_ = 0; resolutionY_override_ = 0;
    }

  protected:

    virtual bool allocateBuffersForStream
    (
        libcamera::Stream *stream
    ) = 0;
    virtual bool configureTriggerMode
    (
        const TriggerMode &mode
    ) = 0;
    virtual cv::Mat convertBufferToMat
    (
        libcamera::FrameBuffer *buffer
    ) = 0;
    virtual void requestComplete
    (
        libcamera::Request *request
    ) = 0;
    virtual void addFrameToBuffer
    (
        const cv::Mat &frame
    ) = 0;

    uint32_t cameraIndex_;

    // Libcamera components
    std::shared_ptr<libcamera::CameraManager> cameraManager_;
    std::shared_ptr<libcamera::Camera> camera_;
    std::unique_ptr<libcamera::FrameBufferAllocator> allocator_;
    std::unique_ptr<libcamera::CameraConfiguration> config_;
    std::vector<std::unique_ptr<libcamera::Request> > requests_;

    // Camera configuration state
    bool isConfigured_;
    bool cameraStarted_ = false;
    TriggerMode triggerMode_;

    // Sensor specifications
    uint32_t currentExposureUs_ = 10000;
    float currentGain_ = 1.0f;
    float currentFps_ = 30.0f;

    int resolutionX_ = 0;
    int resolutionY_ = 0;

    float focalLength_mm_ = 0.0f; // Focal length in mm
    float sensorWidth_mm_ = 0.0f; // Sensor width in mm
    float sensorHeight_mm_ = 0.0f; // Sensor height in mm

    float horizontalFOV_deg_ = 0.0f; // Horizontal field of view in degrees
    float verticalFOV_deg_ = 0.0f; // Vertical field of view in degrees

    cv::Mat calibrationMatrix_;
    cv::Mat distortionCoefficients_;

    int resolutionX_override_ = 0;
    int resolutionY_override_ = 0;

    bool useCalibrationMatrix_ = false;
    bool isCameraOpen_ = false;
    bool isCapturing_ = false;

    // Frame capture synchronization
    std::mutex frameMutex_;
    std::condition_variable frameCondition_;
    cv::Mat latestFrame_;
    bool frameReady_ = false;

    static const std::string cameraModeToString(const TriggerMode &mode)
    {
        switch(mode)
        {
            case TriggerMode::FREE_RUNNING: return "FREE_RUNNING";
            case TriggerMode::EXTERNAL_TRIGGER: return "EXTERNAL_TRIGGER";
            default: return "UNKNOWN";
        }
    }
};
} // namespace PiTrac

#endif // GS_CAMERA_INTERFACE_H