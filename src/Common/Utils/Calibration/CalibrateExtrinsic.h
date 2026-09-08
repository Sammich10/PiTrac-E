#ifndef __CALIBRATE_EXTRINSIC_H__
#define __CALIBRATE_EXTRINSIC_H__

#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/calib3d.hpp>
#include <vector>
#include <string>
#include "Common/Utils/Calibration/CalibrationStruct.h"
#include "Common/Utils/Logging/GSLogger.h"

namespace PiTrac
{
/**
 * @brief Class for calibrating camera extrinsic parameters (position and orientation)
 * using known 3D world points and their corresponding 2D image detections.
 *
 * This class uses cv::solvePnP to compute the camera's rotation and translation
 * vectors relative to a world coordinate system defined by known calibration points
 * (e.g., golf balls at known positions on a calibration rig).
 */
class CalibrateExtrinsic
{
  public:
    // Constants for calibration quality
    static constexpr size_t MIN_CALIBRATION_POINTS = 4; // Minimum points needed for solvePnP
    static constexpr double MAX_REPROJECTION_ERROR = 5.0; // pixels
    static constexpr double REPROJECTION_EXCELLENT = 0.5; // pixels
    static constexpr double REPROJECTION_GOOD = 1.0; // pixels
    static constexpr double REPROJECTION_FAIR = 2.0; // pixels

    enum class CalibrationQuality
    {
        FAILED = 0,
        FAIR = 1,
        GOOD = 2,
        EXCELLENT = 3
    };

    CalibrateExtrinsic();
    ~CalibrateExtrinsic();

    /**
     * @brief Set the camera intrinsics (must be pre-calibrated)
     * @param intrinsics Camera intrinsic parameters (fx, fy, cx, cy)
     */
    void setIntrinsics
    (
        const CameraIntrinsics_Type &intrinsics
    );

    /**
     * @brief Set the distortion coefficients (must be pre-calibrated)
     * @param distortion Distortion coefficients
     * @param isFisheye Whether this is a fisheye distortion model
     */
    void setDistortion
    (
        const std::vector<double> &distortion,
        bool isFisheye = false
    );

    /**
     * @brief Add a known 3D calibration point and its detected 2D image position
     * @param worldPoint 3D point in world coordinates (e.g., ball position on rig)
     * @param imagePoint Detected 2D point in image coordinates
     */
    void addCalibrationPoint
    (
        const cv::Point3d &worldPoint,
        const cv::Point2d &imagePoint
    );

    /**
     * @brief Clear all calibration points
     */
    void clearCalibrationPoints();

    /**
     * @brief Perform extrinsic calibration using solvePnP
     * @param useIterative Use iterative refinement for better accuracy
     * @return Quality of the calibration result
     */
    CalibrationQuality calibrate
    (
        bool useIterative = true
    );

    /**
     * @brief Get the calibration results (rotation and translation vectors)
     * @param rvec Output rotation vector (3x1)
     * @param tvec Output translation vector (3x1)
     * @return Reprojection error in pixels
     */
    double getCalibrationResults
    (
        cv::Mat &rvec,
        cv::Mat &tvec
    ) const;

    /**
     * @brief Get the calibration results as a CameraExtrinsics struct
     * @return CameraExtrinsics structure with rotation, translation, and error
     */
    CameraExtrinsics_Type getExtrinsics() const;

    /**
     * @brief Get the reprojection error for the last calibration
     * @return RMS reprojection error in pixels
     */
    double getReprojectionError() const
    {
        return reprojectionError_;
    }

    /**
     * @brief Get the number of calibration points currently stored
     * @return Number of point pairs
     */
    size_t getNumPoints() const
    {
        return objectPoints_.size();
    }

    /**
     * @brief Project 3D world points to 2D image coordinates using current calibration
     * @param worldPoints 3D points in world coordinates
     * @param imagePoints Output 2D points in image coordinates
     */
    void projectPoints
    (
        const std::vector<cv::Point3d> &worldPoints,
        std::vector<cv::Point2d> &imagePoints
    ) const;

    /**
     * @brief Draw reprojection visualization on an image
     * @param image Input/output image to draw on
     * @param worldPoints 3D points to project
     * @param detectedPoints Detected 2D points (optional, for comparison)
     */
    void drawReprojection
    (
        cv::Mat &image,
        const std::vector<cv::Point3d> &worldPoints,
        const std::vector<cv::Point2d> &detectedPoints = std::vector<cv::Point2d>()
    ) const;

  private:
    std::shared_ptr<GSLogger> logger_;

    // Camera intrinsics and distortion (must be pre-calibrated)
    cv::Mat cameraMatrix_;
    cv::Mat distCoeffs_;
    bool isFisheye_;
    bool hasIntrinsics_;

    // Calibration point pairs
    std::vector<cv::Point3d> objectPoints_;  // 3D world coordinates
    std::vector<cv::Point2d> imagePoints_;   // 2D image coordinates

    // Calibration results
    cv::Mat rvec_;  // Rotation vector (3x1)
    cv::Mat tvec_;  // Translation vector (3x1)
    double reprojectionError_;
    bool calibrated_;

    /**
     * @brief Calculate RMS reprojection error
     * @return RMS error in pixels
     */
    double calculateReprojectionError() const;
};
} // namespace PiTrac

#endif // __CALIBRATE_EXTRINSIC_H__
