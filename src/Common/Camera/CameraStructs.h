#ifndef CAMERA_STRUCTS_H
#define CAMERA_STRUCTS_H

#include <cstdint>
#include <string>
#include <array>
#include <opencv2/opencv.hpp>

namespace PiTrac
{
enum LMCameras
{
    CAMERA_0 = 0,
    CAMERA_1 = 1,
    NUM_CAMERAS = 2,
    MAX_CAMERAS = NUM_CAMERAS,
};

enum CAMERA_TYPE
{
    CAMERA_TYPE_UNKNOWN = 0,
    CAMERA_GENERIC_RASPBERRY_PI,
    CAMERA_PICAM_V3,
    CAMERA_INNOMAKER_IMX296GS,
    CAMERA_TYPE_MAX
};

enum class TriggerMode
{
    FREE_RUNNING = 0,       // Normal continuous capture
    EXTERNAL_TRIGGER        // Wait for external trigger signal
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

typedef struct CameraConfiguration
{
    uint32_t resX;
    uint32_t resY;
    uint32_t sensorWidth_mm;
    uint32_t sensorHeight_mm;
    float focalLength_mm;
} CameraConfiguration_Type;


typedef struct CameraControlSettings
{
    uint32_t exposure_time_us;
    // Optional parameter to adjust exposure when applying calibration (e.g. 0.8 to reduce exposure, 1.0 for no change)
    float analog_gain;
    // Optional parameter to adjust field of view when applying calibration (e.g. 0.8 to preserve more FOV, 1.0 for no change)
    float fov_scale;
} CameraControlSettings_Type;
}

#endif // CAMERA_STRUCTS_H