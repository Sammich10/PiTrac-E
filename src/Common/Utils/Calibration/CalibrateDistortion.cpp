#include "Common/Utils/Calibration/CalibrateDistortion.h"
#include "Common/Utils/Calibration/Detectors/CheckerboardDetector.h"

namespace PiTrac
{
CalibrateDistortion::CalibrateDistortion(const CalibrationModel model)
{
    // Initialize arrays to zero
    calibrationModel_ = model;
    logger_ = GSLogger::getInstance();
    switch(calibrationModel_)
    {
        case CalibrationModel::FISHEYE:
        {
            patternDetector_ = std::make_unique<CheckerboardDetector>(CalibrationModel::FISHEYE);
            doCalibrationFunc_ = std::bind(&CalibrateDistortion::doCalibrationFisheye, this, std::placeholders::_1, std::placeholders::_2);
            break;
        }
        case CalibrationModel::STANDARD:
        default:
        {
            patternDetector_ = std::make_unique<CheckerboardDetector>(CalibrationModel::STANDARD);
            doCalibrationFunc_ = std::bind(&CalibrateDistortion::doCalibrationStandard, this, std::placeholders::_1, std::placeholders::_2);
            break;
        }
    }
}

CalibrateDistortion::~CalibrateDistortion()
{
}

void CalibrateDistortion::setDimensions(const uint32_t rows, const uint32_t cols)
{
    patternDetector_->setDimensions(rows, cols);
}

void CalibrateDistortion::setImageSize(const uint32_t width, const uint32_t height)
{
    patternDetector_->setImageSize(width, height);
}

ImageQuality CalibrateDistortion::processImage(cv::Mat &images, cv::Mat &debugImages, std::vector<cv::Point2f> &corners, std::vector<cv::Point3f> &objectPoints)
{
    // Process the images using the appropriate processing function based on the calibration model
    ImageQuality quality = patternDetector_->processImage(images, debugImages, corners, objectPoints);
    return quality;
}

void CalibrateDistortion::reset()
{
    rvecs_.clear();
    tvecs_.clear();
    cameraMatrix_ = cv::Mat();
    distCoeffs_ = cv::Mat();
}

bool CalibrateDistortion::doDistortionCalibration(std::vector<std::vector<cv::Point2f> > &all_corners, std::vector<std::vector<cv::Point3f> > &all_object_points)
{
    if(all_corners.size() != all_object_points.size() || all_corners.empty() || all_object_points.empty())
    {
        logger_->error("Calibration failed: number of corner sets (%lu) does not match number of object point sets (%lu), or no data provided", all_corners.size(), all_object_points.size());
        return false;
    }
    rvecs_.clear();
    tvecs_.clear();

    logger_->info("Performing calibration");
    double rms = doCalibrationFunc_(all_corners, all_object_points);
    if (rms > REPROJECTION_FAIR)
    {
        logger_->error("Calibration failed with RMS error: " + std::to_string(rms));
        return false;
    }
    reprojectionError_ = getReprojectionError(all_corners, all_object_points, rvecs_, tvecs_);
    logger_->info("Calibration successful with RMS error: " + std::to_string(rms) + " and reprojection error: " + std::to_string(reprojectionError_));
    // Consider calibration successful if RMS error < 1 pixel
    return (rms < REPROJECTION_FAIR);
}

const double CalibrateDistortion::doCalibrationStandard(std::vector<std::vector<cv::Point2f> > &all_corners, std::vector<std::vector<cv::Point3f> > &all_object_points)
{
    logger_->info("Using standard pinhole calibration model");
    // Use flags that allow better distortion coefficient estimation
    // int calibrationFlags = cv::CALIB_RATIONAL_MODEL;
    // No special flags for standard model, can be adjusted as needed
    std::array<size_t, 2> imageSize = patternDetector_->getImageSize();
    int calibrationFlags = 0;
    double rms = cv::calibrateCamera(
        all_object_points,
        all_corners,
        cv::Size(imageSize[0], imageSize[1]),
        cameraMatrix_,
        distCoeffs_,
        rvecs_,
        tvecs_,
        calibrationFlags,
        cv::TermCriteria(cv::TermCriteria::COUNT + cv::TermCriteria::EPS, 100, 1e-6)
        );

    return rms;
}

const double CalibrateDistortion::doCalibrationFisheye(std::vector<std::vector<cv::Point2f> > &all_corners, std::vector<std::vector<cv::Point3f> > &all_object_points)
{
    logger_->info("Using fisheye calibration model");
    // Initialize camera matrix with reasonable estimate based on image size
    // This is critical for fisheye calibration to converge properly
    // Typical for fisheye: focal length ~= 0.6 * width
    std::array<size_t, 2> imageSize = patternDetector_->getImageSize();
    double fx_estimate = imageSize[0] * 0.6;
    double fy_estimate = imageSize[0] * 0.6;
    double cx_estimate = imageSize[0] / 2.0;
    double cy_estimate = imageSize[1] / 2.0;

    cameraMatrix_.at<double>(0, 0) = fx_estimate;
    cameraMatrix_.at<double>(1, 1) = fy_estimate;
    cameraMatrix_.at<double>(0, 2) = cx_estimate;
    cameraMatrix_.at<double>(1, 2) = cy_estimate;

    // Fisheye calibration flags:
    // CALIB_RECOMPUTE_EXTRINSIC: Recompute extrinsic params after each iteration (essential for convergence)
    // CALIB_FIX_SKEW: Fix skew to 0 (assume rectangular pixels)
    // Note: CALIB_CHECK_COND removed - it's too strict and causes failures with valid calibration data
    int calibrationFlags = cv::fisheye::CALIB_RECOMPUTE_EXTRINSIC | cv::fisheye::CALIB_FIX_SKEW;

    double rms = cv::fisheye::calibrate(
        all_object_points,
        all_corners,
        cv::Size(imageSize[0], imageSize[1]),
        cameraMatrix_,
        distCoeffs_,
        rvecs_,
        tvecs_,
        calibrationFlags,
        cv::TermCriteria(cv::TermCriteria::COUNT + cv::TermCriteria::EPS, 100, 1e-6)
        );

    return rms;
}

double CalibrateDistortion::getReprojectionError(std::vector<std::vector<cv::Point2f> > &all_corners,
                                                 std::vector<std::vector<cv::Point3f> > &all_object_points,
                                                 std::vector<cv::Mat> &rvecs,
                                                 std::vector<cv::Mat> &tvecs) const
{
    if (all_object_points.empty() || all_corners.empty() || cameraMatrix_.empty())
    {
        return -1.0; // Invalid
    }

    std::vector<cv::Point2f> imagePoints2;
    double totalError = 0;
    int totalPoints = 0;

    for (size_t i = 0; i < all_object_points.size(); i++)
    {
        switch(calibrationModel_)
        {
            case CalibrationModel::FISHEYE:
            {
                cv::fisheye::projectPoints(all_object_points[i], imagePoints2, rvecs[i], tvecs[i],
                                           cameraMatrix_, distCoeffs_);
                break;
            }
            case CalibrationModel::STANDARD:
            default:
            {
                cv::projectPoints(all_object_points[i], rvecs[i], tvecs[i], cameraMatrix_,
                                  distCoeffs_, imagePoints2);
                break;
            }
        }
        double err = cv::norm(all_corners[i], imagePoints2, cv::NORM_L2);
        totalError += err * err;
        totalPoints += static_cast<int>(all_object_points[i].size());
    }

    return std::sqrt(totalError / totalPoints);
}
} // End namespace PiTrac