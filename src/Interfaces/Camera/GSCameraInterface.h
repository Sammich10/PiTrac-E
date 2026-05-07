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
#include <functional>
#include "Common/Camera/CameraStructs.h"

namespace PiTrac
{
// Callback type for request completion, e.g., for handling captured frames
using requestCompleteCallback = std::function<void (cv::Mat &frame)>;
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
 * The camera interface supports camera life-cycle operations:
 *  - openCamera() : Acquire the camera device
 *  - configureStream() : Configure the camera stream for specific use cases,
 * allocate buffers
 *  - start() : Begin capturing frames
 *  - stop() : Stop capturing frames
 *  - closeCamera() : Release the camera device
 *
 */
  public:
    struct CameraInfo
    {
        std::string model;
        std::string id;
    };

    struct CameraUUIDInfo
    {
        std::string uuid;
        uint32_t uuid_length;
        bool isValid() const
        {
            return !uuid.empty() && uuid_length > 0;
        }
    };

    // You can also provide a protected constructor with common parameters
    GSCameraInterface(const uint32_t &cameraIndex, std::shared_ptr<libcamera::CameraManager> const &cameraManager)
        : cameraIndex_(cameraIndex)
        , resolutionX_(0)
        , resolutionY_(0)
        , focalLength_mm_(0)
        , cameraManager_(std::move(cameraManager))
        , camera_(nullptr)
        , allocator_(nullptr)
        , isConfigured_(false)
        , triggerMode_(TriggerMode::FREE_RUNNING)
        , isCapturing_(false)
    {
    }

    virtual ~GSCameraInterface() = default;

    /** Pure virtual methods to be implemented by derived classes **/
    virtual bool initialize() = 0;
    virtual bool openCamera() = 0;
    virtual bool configureStream
    (
        const libcamera::StreamRole &streamRole
    ) = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual void closeCamera() = 0;
    virtual cv::Mat captureFrame
    (
        uint32_t timeout_ms = 2000
    ) = 0;
    virtual CAMERA_TYPE getCameraType() const = 0;
    virtual bool setTriggerMode
    (
        TriggerMode mode
    ) = 0;
    virtual bool startContinuousCapture
    (
        requestCompleteCallback callback
    ) = 0;
    virtual bool stopContinuousCapture() = 0;
    virtual std::string toString() const = 0;

    /** Accessor methods **/

    bool isInitialized() const
    {
        return isInitialized_;
    }

    uint32_t getCameraIndex() const
    {
        return cameraIndex_;
    }

    uint32_t getNumBuffers() const
    {
        return numBuffers_;
    }

    int getResolutionX() const
    {
        return resolutionX_;
    }

    int getResolutionY() const
    {
        return resolutionY_;
    }

    libcamera::PixelFormat getPixelFormat() const
    {
        return pixelFormat_;
    }

    libcamera::Orientation getSensorOrientation() const
    {
        return sensorOrientation_;
    }

    unsigned int getStride() const
    {
        return stride_;
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
        return analogGain_;
    }

    float getDigitalGain() const
    {
        return digitalGain_;
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

    CameraInfo getInfo() const
    {
        return camInfo_;
    }

    CameraUUIDInfo getUUIDInfo() const
    {
        return uuidInfo_;
    }

    /** Mutator methods **/

    void setNumBuffers
    (
        uint32_t numBuffers
    )
    {
        numBuffers_ = numBuffers;
    }

    void setResolution
    (
        int resX, int resY
    )
    {
        resolutionX_ = resX; resolutionY_ = resY;
    }

    void setPixelFormat
    (
        libcamera::PixelFormat pixelFormat
    )
    {
        pixelFormat_ = pixelFormat;
    }

    void setOrientation
    (
        libcamera::Orientation orientation
    )
    {
        sensorOrientation_ = orientation;
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

    virtual bool setExposureTime
    (
        uint32_t exposureUs
    )
    {
        currentExposureUs_ = exposureUs;
        return true;
    }

    virtual bool setAnalogGain
    (
        float gain
    )
    {
        analogGain_ = gain; return true;
    }

    bool setDigitalGain
    (
        float gain
    )
    {
        digitalGain_ = gain; return true;
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
        int resX,
        int resY
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

    struct CameraI2CInfo
    {
        int busNumber;
        uint8_t deviceAddress;
        std::string devicePath;
        std::string deviceTreePath;
        bool isValid() const
        {
            return busNumber != -1 && deviceAddress != 0;
        }
    };

    virtual bool allocateBuffersForStream
    (
        libcamera::Stream *stream
    ) = 0;

    virtual bool configureTriggerMode
    (
        const TriggerMode &mode
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
    bool isConfigured_ = false;
    bool cameraStarted_ = false;
    TriggerMode triggerMode_;
    uint32_t numBuffers_ = 4;

    // Sensor specifications
    uint32_t currentExposureUs_ = 10000;
    float analogGain_ = 1.0f;
    float digitalGain_ = 1.0f;
    float currentFps_ = 30.0f;

    int resolutionX_ = 0;
    int resolutionY_ = 0;
    libcamera::PixelFormat pixelFormat_ = libcamera::formats::BGR888;
    libcamera::Orientation sensorOrientation_ = libcamera::Orientation::Rotate0;

    float focalLength_mm_ = 0.0f; // Focal length in mm
    float sensorWidth_mm_ = 0.0f; // Sensor width in mm
    float sensorHeight_mm_ = 0.0f; // Sensor height in mm

    float horizontalFOV_deg_ = 0.0f; // Horizontal field of view in degrees
    float verticalFOV_deg_ = 0.0f; // Vertical field of view in degrees

    // Stride in bytes, derived from pixel format and resolution once
    // configured, immutable through
    // camera interface, set during stream configuration
    unsigned int stride_ = 0;
    unsigned int frameSizeBytes_ = 0;

    cv::Mat calibrationMatrix_;
    cv::Mat distortionCoefficients_;

    int resolutionX_override_ = 0;
    int resolutionY_override_ = 0;

    bool useCalibrationMatrix_ = false;
    bool isCameraOpen_ = false;
    bool isCapturing_ = false;
    bool isInitialized_ = false;

    // Frame capture synchronization
    std::mutex frameMutex_;
    std::condition_variable frameCondition_;
    cv::Mat latestFrame_;
    bool frameReady_ = false;
    // Callback for request completion
    requestCompleteCallback requestCallback_ = nullptr;

    CameraI2CInfo i2cInfo_;
    CameraInfo camInfo_;
    CameraUUIDInfo uuidInfo_;

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