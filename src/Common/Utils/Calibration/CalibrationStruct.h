#ifndef __CALIBRATION_STRUCTS_H__
#define __CALIBRATION_STRUCTS_H__

#include <string>

namespace PiTrac
{
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
    std::string calibration_type; // e.g. "intrinsics", "distortion"
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
} // namespace PiTrac

#endif // __CALIBRATION_STRUCTS_H__