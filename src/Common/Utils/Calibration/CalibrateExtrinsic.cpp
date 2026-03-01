#include "Common/Utils/Calibration/CalibrateExtrinsic.h"
#include <opencv2/calib3d.hpp>
#include <cmath>

namespace PiTrac
{
CalibrateExtrinsic::CalibrateExtrinsic()
    : logger_(nullptr)
    , isFisheye_(false)
    , hasIntrinsics_(false)
    , reprojectionError_(0.0)
    , calibrated_(false)
{
    logger_ = GSLogger::getInstance();
    cameraMatrix_ = cv::Mat::eye(3, 3, CV_64F);
    distCoeffs_ = cv::Mat::zeros(4, 1, CV_64F);
    rvec_ = cv::Mat::zeros(3, 1, CV_64F);
    tvec_ = cv::Mat::zeros(3, 1, CV_64F);
}

CalibrateExtrinsic::~CalibrateExtrinsic()
{
}

void CalibrateExtrinsic::setIntrinsics(const CameraIntrinsics_Type &intrinsics)
{
    if (!intrinsics.valid())
    {
        logger_->error("CalibrateExtrinsic: Invalid camera intrinsics provided");
        return;
    }

    cameraMatrix_ = (cv::Mat_<double>(3, 3) <<
                     intrinsics.focal_length_x, 0, intrinsics.principal_point_x,
                     0, intrinsics.focal_length_y, intrinsics.principal_point_y,
                     0, 0, 1);

    hasIntrinsics_ = true;
    logger_->info("CalibrateExtrinsic: Camera intrinsics set - fx:" +
                  std::to_string(intrinsics.focal_length_x) + ", fy:" +
                  std::to_string(intrinsics.focal_length_y));
}

void CalibrateExtrinsic::setDistortion(const std::vector<double> &distortion, bool isFisheye)
{
    if (distortion.empty())
    {
        logger_->warning("CalibrateExtrinsic: Empty distortion coefficients provided");
        distCoeffs_ = cv::Mat::zeros(4, 1, CV_64F);
        return;
    }

    isFisheye_ = isFisheye;
    distCoeffs_ = cv::Mat(distortion).clone();

    logger_->info("CalibrateExtrinsic: Distortion coefficients set (" +
                  std::string(isFisheye ? "fisheye" : "standard") + " model, " +
                  std::to_string(distortion.size()) + " coefficients)");
}

void CalibrateExtrinsic::addCalibrationPoint(const cv::Point3d &worldPoint, const cv::Point2d &imagePoint)
{
    objectPoints_.push_back(worldPoint);
    imagePoints_.push_back(imagePoint);
}

void CalibrateExtrinsic::clearCalibrationPoints()
{
    objectPoints_.clear();
    imagePoints_.clear();
    calibrated_ = false;
    reprojectionError_ = 0.0;
}

CalibrateExtrinsic::CalibrationQuality CalibrateExtrinsic::calibrate(bool useIterative)
{
    if (!hasIntrinsics_)
    {
        logger_->error("CalibrateExtrinsic: Camera intrinsics not set. Call setIntrinsics() first.");
        return CalibrationQuality::FAILED;
    }

    if (objectPoints_.size() < MIN_CALIBRATION_POINTS)
    {
        logger_->error("CalibrateExtrinsic: Insufficient calibration points. Need at least " +
                       std::to_string(MIN_CALIBRATION_POINTS) + ", have " +
                       std::to_string(objectPoints_.size()));
        return CalibrationQuality::FAILED;
    }

    if (objectPoints_.size() != imagePoints_.size())
    {
        logger_->error("CalibrateExtrinsic: Mismatch between object points and image points");
        return CalibrationQuality::FAILED;
    }

    logger_->info("CalibrateExtrinsic: Starting calibration with " +
                  std::to_string(objectPoints_.size()) + " points...");

    try
    {
        // Use solvePnP to compute camera pose
        // SOLVEPNP_ITERATIVE provides better accuracy through refinement
        int flags = useIterative ? cv::SOLVEPNP_ITERATIVE : cv::SOLVEPNP_EPNP;

        bool success = cv::solvePnP(
            objectPoints_,
            imagePoints_,
            cameraMatrix_,
            distCoeffs_,
            rvec_,
            tvec_,
            false,  // useExtrinsicGuess
            flags
            );

        if (!success)
        {
            logger_->error("CalibrateExtrinsic: solvePnP failed to converge");
            return CalibrationQuality::FAILED;
        }

        // Calculate reprojection error
        reprojectionError_ = calculateReprojectionError();
        calibrated_ = true;

        logger_->info("CalibrateExtrinsic: Calibration successful!");
        logger_->info("  Rotation (rvec): [" + std::to_string(rvec_.at<double>(0)) + ", " +
                      std::to_string(rvec_.at<double>(1)) + ", " +
                      std::to_string(rvec_.at<double>(2)) + "]");
        logger_->info("  Translation (tvec): [" + std::to_string(tvec_.at<double>(0)) + ", " +
                      std::to_string(tvec_.at<double>(1)) + ", " +
                      std::to_string(tvec_.at<double>(2)) + "]");
        logger_->info("  Reprojection error: " + std::to_string(reprojectionError_) + " pixels");

        // Determine quality based on reprojection error
        if (reprojectionError_ <= REPROJECTION_EXCELLENT)
        {
            logger_->info("  Quality: EXCELLENT");
            return CalibrationQuality::EXCELLENT;
        }
        else if (reprojectionError_ <= REPROJECTION_GOOD)
        {
            logger_->info("  Quality: GOOD");
            return CalibrationQuality::GOOD;
        }
        else if (reprojectionError_ <= REPROJECTION_FAIR)
        {
            logger_->info("  Quality: FAIR");
            return CalibrationQuality::FAIR;
        }
        else
        {
            logger_->warning("  Quality: POOR (error > " + std::to_string(REPROJECTION_FAIR) + " pixels)");
            return CalibrationQuality::FAIR;
        }
    }
    catch (const cv::Exception &e)
    {
        logger_->error("CalibrateExtrinsic: OpenCV exception during calibration: " + std::string(e.what()));
        return CalibrationQuality::FAILED;
    }
}

double CalibrateExtrinsic::getCalibrationResults(cv::Mat &rvec, cv::Mat &tvec) const
{
    if (!calibrated_)
    {
        logger_->warning("CalibrateExtrinsic: Attempting to get results before calibration");
        return -1.0;
    }

    rvec = rvec_.clone();
    tvec = tvec_.clone();
    return reprojectionError_;
}

CameraExtrinsics_Type CalibrateExtrinsic::getExtrinsics() const
{
    CameraExtrinsics_Type extrinsics;

    if (calibrated_)
    {
        extrinsics.rvec[0] = rvec_.at<double>(0);
        extrinsics.rvec[1] = rvec_.at<double>(1);
        extrinsics.rvec[2] = rvec_.at<double>(2);

        extrinsics.tvec[0] = tvec_.at<double>(0);
        extrinsics.tvec[1] = tvec_.at<double>(1);
        extrinsics.tvec[2] = tvec_.at<double>(2);

        extrinsics.reprojection_error = reprojectionError_;
        extrinsics.num_points = static_cast<int>(objectPoints_.size());
    }

    return extrinsics;
}

void CalibrateExtrinsic::projectPoints(const std::vector<cv::Point3d> &worldPoints,
                                       std::vector<cv::Point2d> &imagePoints) const
{
    if (!calibrated_)
    {
        logger_->warning("CalibrateExtrinsic: Attempting to project points before calibration");
        return;
    }

    if (worldPoints.empty())
    {
        return;
    }

    std::vector<cv::Point2f> projectedPoints;
    cv::projectPoints(worldPoints, rvec_, tvec_, cameraMatrix_, distCoeffs_, projectedPoints);

    imagePoints.clear();
    imagePoints.reserve(projectedPoints.size());
    for (const auto &pt : projectedPoints)
    {
        imagePoints.push_back(cv::Point2d(pt.x, pt.y));
    }
}

void CalibrateExtrinsic::drawReprojection(cv::Mat &image,
                                          const std::vector<cv::Point3d> &worldPoints,
                                          const std::vector<cv::Point2d> &detectedPoints) const
{
    if (!calibrated_ || image.empty())
    {
        return;
    }

    // Project world points to image
    std::vector<cv::Point2d> projectedPoints;
    projectPoints(worldPoints, projectedPoints);

    // Draw projected points in green
    for (const auto &pt : projectedPoints)
    {
        cv::circle(image, pt, 8, cv::Scalar(0, 255, 0), 2);
        cv::circle(image, pt, 2, cv::Scalar(0, 255, 0), -1);
    }

    // If detected points are provided, draw them in blue and show error lines
    if (!detectedPoints.empty() && detectedPoints.size() == projectedPoints.size())
    {
        for (size_t i = 0; i < detectedPoints.size(); i++)
        {
            // Draw detected point in blue
            cv::circle(image, detectedPoints[i], 8, cv::Scalar(255, 0, 0), 2);
            cv::circle(image, detectedPoints[i], 2, cv::Scalar(255, 0, 0), -1);

            // Draw line showing reprojection error
            cv::line(image, detectedPoints[i], projectedPoints[i], cv::Scalar(0, 255, 255), 1);

            // Calculate and display error for this point
            double error = cv::norm(detectedPoints[i] - projectedPoints[i]);
            cv::putText(image,
                        cv::format("%.2f px", error),
                        cv::Point(projectedPoints[i].x + 10, projectedPoints[i].y - 10),
                        cv::FONT_HERSHEY_SIMPLEX,
                        0.4,
                        cv::Scalar(255, 255, 255),
                        1);
        }
    }

    // Display overall reprojection error
    std::string errorText = "RMS Error: " + cv::format("%.3f", reprojectionError_) + " px";
    cv::putText(image, errorText, cv::Point(10, 30),
                cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 2);
}

double CalibrateExtrinsic::calculateReprojectionError() const
{
    if (objectPoints_.empty() || imagePoints_.empty())
    {
        return 0.0;
    }

    // Project object points using current rvec and tvec
    std::vector<cv::Point2d> projectedPoints;
    projectPoints(objectPoints_, projectedPoints);

    // Calculate RMS error
    double sumSquaredError = 0.0;
    for (size_t i = 0; i < imagePoints_.size(); i++)
    {
        double error = cv::norm(imagePoints_[i] - projectedPoints[i]);
        sumSquaredError += error * error;
    }

    return std::sqrt(sumSquaredError / imagePoints_.size());
}
} // namespace PiTrac
