#ifndef __CALIBRATION_STRUCTS_H__
#define __CALIBRATION_STRUCTS_H__

#include <string>
#include <array>
#include <opencv2/opencv.hpp>
#include "Common/Camera/CameraStructs.h"

namespace PiTrac
{
// Constants for calibration limits and thresholds
static constexpr size_t MAX_CALIBRATION_IMAGES = 100;
static constexpr size_t MIN_CALIBRATION_IMAGES = 3;
static constexpr size_t MAX_CHECKERBOARD_ROWS = 20;
static constexpr size_t MAX_CHECKERBOARD_COLS = 20;
static constexpr size_t MIN_CHECKERBOARD_ROWS = 2;
static constexpr size_t MIN_CHECKERBOARD_COLS = 2;
static constexpr double MAX_REPROJECTION_ERROR = 5.0;
static constexpr double REPROJECTION_EXCELLENT = 0.3;
static constexpr double REPROJECTION_GOOD = 0.6;
static constexpr double REPROJECTION_FAIR = 1.0;
static constexpr double REPROJECTION_POOR = 2.0;
static constexpr double CHECKERBOARD_COVERAGE_TOO_LOW = 0.2;
static constexpr double CHECKERBOARD_COVERAGE_GOOD = 0.5;
static constexpr double CHECKERBOARD_COVERAGE_EXCELLENT = 0.8;
static constexpr double CHECKERBOARD_COVERAGE_TOO_HIGH = 0.95;
static constexpr double SHARPNESS_THRESHOLD_LOW = 50.0;
static constexpr double SHARPNESS_THRESHOLD_HIGH = 100.0;
static constexpr double MIN_CORNER_CLUSTERING = 0.15;
static constexpr double CORNER_CLUSTERING_GOOD = 0.20;
static constexpr double CORNER_CLUSTERING_EXCELLENT = 0.25;

/**
 * @brief Enumeration for different camera distortion models
 */
enum class CalibrationModel
{
    STANDARD,  ///< Standard radial-tangential model (k1,k2,p1,p2,k3)
    FISHEYE    ///< Fisheye model (k1,k2,k3,k4) - better for wide FOV cameras
};

enum class ImageQuality
{
    REJECTED = 0,
    POOR = 1,
    GOOD = 2,
    VERY_GOOD = 3,
    EXCELLENT = 4,
};

typedef struct CameraControlSettings
{
    uint32_t exposure_time_us;
    float analog_gain;
    float fov_scale; // Optional parameter to adjust field of view when applying calibration (e.g. 0.8 to preserve more FOV, 1.0 for no change)
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
    CameraIntrinsics(std::array<double, 4> intrinsics)
    {
        focal_length_x = intrinsics[0];
        focal_length_y = intrinsics[1];
        principal_point_x = intrinsics[2];
        principal_point_y = intrinsics[3];
        mat = (cv::Mat_<double>(3, 3) << focal_length_x, 0.0, principal_point_x, 0.0, focal_length_y, principal_point_y, 0.0, 0.0, 1.0);
        mat_scaled = mat.clone();
    }

    CameraIntrinsics(const cv::Mat cameraMatrix)
    {
        if (cameraMatrix.rows == 3 && cameraMatrix.cols == 3)
        {
            focal_length_x = cameraMatrix.at<double>(0, 0);
            focal_length_y = cameraMatrix.at<double>(1, 1);
            principal_point_x = cameraMatrix.at<double>(0, 2);
            principal_point_y = cameraMatrix.at<double>(1, 2);
            mat = cameraMatrix.clone();
            mat_scaled = cameraMatrix.clone();
        }
        else
        {
            focal_length_x = focal_length_y = principal_point_x = principal_point_y = 0.0;
        }
    }

    CameraIntrinsics() : focal_length_x(0.0), focal_length_y(0.0), principal_point_x(0.0), principal_point_y(0.0)
    {
        mat = cv::Mat::zeros(3, 3, CV_64F);
        mat_scaled = cv::Mat::zeros(3, 3, CV_64F);
    }

    double focal_length_x; // fx
    double focal_length_y; // fy
    double principal_point_x; // cx
    double principal_point_y; // cy

    cv::Mat mat;
    cv::Mat mat_scaled;

    bool valid() const
    {
        // A simple validity check could be that focal lengths are non-zero
        return (focal_length_x != 0.0 && focal_length_y != 0.0);
    }

    // Return intrinsics as an array for easy storage or conversion
    std::array<double, 4> toArray() const
    {
        return {focal_length_x, focal_length_y, principal_point_x, principal_point_y};
    }

    // Convert intrinsics to OpenCV camera matrix format
    cv::Mat toMat() const
    {
        return mat;
    }

    cv::Mat &toMat()
    {
        return mat;
    }

    cv::Mat toMatScaled() const
    {
        return mat_scaled;
    }

    cv::Mat &toMatScaled()
    {
        return mat_scaled;
    }

    void scaleF(const double scale_x, const double scale_y)
    {
        mat_scaled = mat.clone();
        // Note: We are only scaling the focal lengths here for FOV scaling, not the principal point, as that is
        // a common approach for simulating a digital zoom effect. However, depending on the desired effect, we may
        // also want to scale the principal point accordingly, especially if we begin to support configurable
        // cropping or zooming in the future. For now, we will keep it simple and only scale the focal lengths.
        mat_scaled.at<double>(0, 0) = focal_length_x * scale_x;
        mat_scaled.at<double>(1, 1) = focal_length_y * scale_y;
    }

    void scaleF(const double scale)
    {
        scaleF(scale, scale);
    }
} CameraIntrinsics_Type;

typedef struct DistortionCoefficients
{   // OpenCV typically uses 5 distortion coefficients: k1, k2, p1, p2, k3
    DistortionCoefficients(std::array<double, 5> arrayCoeffs)
    {
        coeffs.standard.k1 = arrayCoeffs[0];
        coeffs.standard.k2 = arrayCoeffs[1];
        coeffs.standard.p1 = arrayCoeffs[2];
        coeffs.standard.p2 = arrayCoeffs[3];
        coeffs.standard.k3 = arrayCoeffs[4];
        model = CalibrationModel::STANDARD;
        mat = (cv::Mat_<double>(5, 1) << coeffs.standard.k1, coeffs.standard.k2, coeffs.standard.p1, coeffs.standard.p2, coeffs.standard.k3);
    }

    DistortionCoefficients(const std::array<double, 4> arrayCoeffs)
    {
        coeffs.fisheye.k1 = arrayCoeffs[0];
        coeffs.fisheye.k2 = arrayCoeffs[1];
        coeffs.fisheye.k3 = arrayCoeffs[2]; // For fisheye, we can store k3 in the k3 slot and ignore p1/p2
        coeffs.fisheye.k4 = arrayCoeffs[3]; // For fisheye, we can store k4 in an extended slot
        model = CalibrationModel::FISHEYE;
        mat = (cv::Mat_<double>(4, 1) << coeffs.fisheye.k1, coeffs.fisheye.k2, coeffs.fisheye.k3, coeffs.fisheye.k4);
    }

    DistortionCoefficients(const cv::Mat &distCoeffs, CalibrationModel calibModel)
    {
        switch(calibModel)
        {
            case CalibrationModel::STANDARD:
                if (distCoeffs.rows >= 5)
                {
                    coeffs.standard.k1 = distCoeffs.at<double>(0, 0);
                    coeffs.standard.k2 = distCoeffs.at<double>(1, 0);
                    coeffs.standard.p1 = distCoeffs.at<double>(2, 0);
                    coeffs.standard.p2 = distCoeffs.at<double>(3, 0);
                    coeffs.standard.k3 = distCoeffs.at<double>(4, 0);
                    model = CalibrationModel::STANDARD;
                    mat = distCoeffs.clone();
                }
                else
                {
                    // Invalid input - initialize to zero
                    coeffs.standard.k1 = coeffs.standard.k2 = coeffs.standard.p1 = coeffs.standard.p2 = coeffs.standard.k3 = 0.0;
                    model = calibModel; // Set model even if coefficients are invalid
                }
                break;
            case CalibrationModel::FISHEYE:
                if (distCoeffs.rows >= 4)
                {
                    coeffs.fisheye.k1 = distCoeffs.at<double>(0, 0);
                    coeffs.fisheye.k2 = distCoeffs.at<double>(1, 0);
                    coeffs.fisheye.k3 = distCoeffs.at<double>(2, 0);
                    coeffs.fisheye.k4 = distCoeffs.at<double>(3, 0);
                    model = CalibrationModel::FISHEYE;
                    mat = distCoeffs.clone();
                }
                else
                {
                    // Invalid input - initialize to zero
                    coeffs.fisheye.k1 = coeffs.fisheye.k2 = coeffs.fisheye.k3 = coeffs.fisheye.k4 = 0.0;
                    model = calibModel; // Set model even if coefficients are invalid
                }
                break;
            default:
                // Invalid input - initialize all to zero
                coeffs.standard.k1 = coeffs.standard.k2 = coeffs.standard.p1 = coeffs.standard.p2 = coeffs.standard.k3 = 0.0;
                coeffs.fisheye.k1 = coeffs.fisheye.k2 = coeffs.fisheye.k3 = coeffs.fisheye.k4 = 0.0;
                model = calibModel; // Set model even if coefficients are invalid
                mat = cv::Mat::zeros(5, 1, CV_64F); // Initialize mat to zeros
        }
    }

    DistortionCoefficients() : model(CalibrationModel::STANDARD)
    {
        coeffs.standard.k1 = coeffs.standard.k2 = coeffs.standard.p1 = coeffs.standard.p2 = coeffs.standard.k3 = 0.0;
        coeffs.fisheye.k1 = coeffs.fisheye.k2 = coeffs.fisheye.k3 = coeffs.fisheye.k4 = 0.0;
    }

    struct StandardCoeffs
    {
        double k1;
        double k2;
        double p1;
        double p2;
        double k3;
    };
    struct FisheyeCoeffs
    {
        double k1;
        double k2;
        double k3;
        double k4;
    };
    union Coefficients
    {
        StandardCoeffs standard;
        FisheyeCoeffs fisheye;
    };

    Coefficients coeffs;

    CalibrationModel model; // Indicates which distortion model these coefficients correspond to

    cv::Mat mat;

    std::array<double, 5> toArray() const
    {
        if (model == CalibrationModel::STANDARD)
        {
            return {coeffs.standard.k1, coeffs.standard.k2, coeffs.standard.p1, coeffs.standard.p2, coeffs.standard.k3};
        }
        else if (model == CalibrationModel::FISHEYE)
        {
            // For fisheye, we can store k3 in the k3 slot and ignore p1/p2
            return {coeffs.fisheye.k1, coeffs.fisheye.k2, coeffs.fisheye.k3, coeffs.fisheye.k4, 0.0};
        }
        else
        {
            return {0.0, 0.0, 0.0, 0.0, 0.0}; // Invalid model
        }
    }

    std::array<double, 4> toArrayFisheye() const
    {
        if (model == CalibrationModel::FISHEYE)
        {
            return {coeffs.fisheye.k1, coeffs.fisheye.k2, coeffs.fisheye.k3, coeffs.fisheye.k4};
        }
        else
        {
            return {0.0, 0.0, 0.0, 0.0}; // Invalid model for fisheye array
        }
    }

    cv::Mat toMat() const
    {
        return mat;
    }

    cv::Mat &toMat()
    {
        return mat;
    }

    bool valid() const
    {
        switch(model)
        {
            // A simple validity check could be that at least one coefficient is non-zero
            case CalibrationModel::FISHEYE:
                return (coeffs.fisheye.k1 != 0.0 || coeffs.fisheye.k2 != 0.0 || coeffs.fisheye.k3 != 0.0 || coeffs.fisheye.k4 != 0.0);
            case CalibrationModel::STANDARD:
                return (coeffs.standard.k1 != 0.0 || coeffs.standard.k2 != 0.0 || coeffs.standard.p1 != 0.0 || coeffs.standard.p2 != 0.0 || coeffs.standard.k3 != 0.0);
            default:
                return false;
        }
    }

    CalibrationModel getModel() const
    {
        return model;
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
} // namespace PiTrac

#endif // __CALIBRATION_STRUCTS_H__