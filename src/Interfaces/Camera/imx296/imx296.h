#ifndef IMX296_CAMERA_H
#define IMX296_CAMERA_H

#include "Interfaces/Camera/GSCameraInterface.h"
#include "Interfaces/Camera/GSCameraBase/GSCameraBase.h"

namespace PiTrac
{
class IMX296Camera : public GSCameraBase
{
  public:

    /**
     * @brief Constructs an IMX296Camera object with specified parameters.
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
    IMX296Camera(const uint32_t &cameraIndex, std::shared_ptr<libcamera::CameraManager> const &cameraManager)
        : GSCameraBase(cameraIndex, cameraManager)
    {
    }

    /**
     * @brief Gets the type of the camera.
     *
     * @return The camera type.
     */
    CAMERA_TYPE getCameraType() const final
    {
        return CAMERA_TYPE::CAMERA_INNOMAKER_IMX296GS;
    }

    /**
     * @brief Configures the trigger mode for image acquisition.
     *
     * @return True if the trigger mode was set successfully, false otherwise.
     */
    bool configureTriggerMode
    (
        const TriggerMode &mode
    ) final;

    /**
     * @brief Provides a string representation of the camera and its current
     * settings.
     *
     * @return A string describing the camera.
     */
    std::string toString() const final;

  private:
    // Private member variables specific to the IMX296 camera can be added here.
}; // class IMX296Camera
} // namespace PiTrac

#endif // IMX296_CAMERA_H