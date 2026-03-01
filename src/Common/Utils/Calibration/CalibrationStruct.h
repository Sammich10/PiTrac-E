#ifndef __CALIBRATION_STRUCTS_H__
#define __CALIBRATION_STRUCTS_H__

#include <string>
#include <array>

namespace PiTrac
{
/**
 * @brief Enumeration for different camera distortion models
 */
enum class CalibrationModel
{
    STANDARD,  ///< Standard radial-tangential model (k1,k2,p1,p2,k3)
    FISHEYE    ///< Fisheye model (k1,k2,k3,k4) - better for wide FOV cameras
};

typedef struct CameraControlSettings
{
    uint32_t exposure_time_us;
    float analog_gain;
    float fov_scale; // Optional parameter to adjust field of view when applying
                     // calibration (e.g. 0.8 to preserve more FOV, 1.0 for no
                     // change)
} CameraControlSettings_Type;

typedef struct CameraInfo
{
    uint32_t camera_id;
    std::string camera_name;
    std::string camera_type;
    std::string uuid;
} CameraInfo_Type;

typedef struct CalibrationEntry
{
    uint32_t calibration_id;
    CalibrationModel calibration_type;
    std::string calibration_date; // ISO 8601 format
    double reprojection_error; // Average re-projection error for this
                               // calibration
} CalibrationEntry_Type;

typedef struct CameraIntrinsics
{
    CameraIntrinsics(std::array<double, 4> intrinsics = {0.0, 0.0, 0.0, 0.0})
    {
        focal_length_x = intrinsics[0];
        focal_length_y = intrinsics[1];
        principal_point_x = intrinsics[2];
        principal_point_y = intrinsics[3];
    }

    double focal_length_x; // fx
    double focal_length_y; // fy
    double principal_point_x; // cx
    double principal_point_y; // cy
    bool valid() const
    {
        // A simple validity check could be that focal lengths are non-zero
        return (focal_length_x != 0.0 && focal_length_y != 0.0);
    }
} CameraIntrinsics_Type;

typedef struct DistortionCoefficients
{   // OpenCV typically uses 5 distortion coefficients: k1, k2, p1, p2, k3
    DistortionCoefficients(std::array<double, 5> coeffs = {0.0, 0.0, 0.0, 0.0, 0.0})
    {
        k1 = coeffs[0];
        k2 = coeffs[1];
        p1 = coeffs[2];
        p2 = coeffs[3];
        k3 = coeffs[4];
    }

    double k1; // Radial distortion coefficient k1
    double k2; // Radial distortion coefficient k2
    double p1; // Tangential distortion coefficient p1
    double p2; // Tangential distortion coefficient p2
    double k3; // Radial distortion coefficient k3 (optional, may be 0 if not
               // used)
    bool valid() const
    {
        // A simple validity check could be that at least one coefficient is
        // non-zero
        return (k1 != 0.0 || k2 != 0.0 || p1 != 0.0 || p2 != 0.0 || k3 != 0.0);
    }
} DistortionCoefficients_Type;

typedef struct CameraExtrinsics
{
    CameraExtrinsics()
    {
        rvec[0] = rvec[1] = rvec[2] = 0.0;
        tvec[0] = tvec[1] = tvec[2] = 0.0;
        reprojection_error = 0.0;
        num_points = 0;
    }

    double rvec[3]; // Rotation vector (Rodrigues format)
    double tvec[3]; // Translation vector (camera position in world coordinates)
    double reprojection_error; // Average reprojection error for this
                               // calibration
    int num_points; // Number of calibration points used

    bool valid() const
    {
        // Check if calibration has been performed (non-zero translation or
        // rotation)
        return (tvec[0] != 0.0 || tvec[1] != 0.0 || tvec[2] != 0.0 ||
                rvec[0] != 0.0 || rvec[1] != 0.0 || rvec[2] != 0.0);
    }
} CameraExtrinsics_Type;

typedef struct FisheyeDistortionCoefficients
{
    FisheyeDistortionCoefficients(std::array<double, 4> coeffs = {0.0, 0.0, 0.0, 0.0})
    {
        k1 = coeffs[0];
        k2 = coeffs[1];
        k3 = coeffs[2];
        k4 = coeffs[3];
    }

    double k1; // Fisheye distortion coefficient k1
    double k2; // Fisheye distortion coefficient k2
    double k3; // Fisheye distortion coefficient k3
    double k4; // Fisheye distortion coefficient k4
    bool valid() const
    {
        // A simple validity check could be that at least one coefficient is
        // non-zero
        return (k1 != 0.0 || k2 != 0.0 || k3 != 0.0 || k4 != 0.0);
    }
} FisheyeDistortionCoefficients_Type;
} // namespace PiTrac

#endif // __CALIBRATION_STRUCTS_H__