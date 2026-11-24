#include "Common/Utils/Calibration/CalibrationUtils.h"

namespace PiTrac
{
std::shared_ptr<GSLogger> CalUtils::logger_ = GSLogger::getInstance();

CalUtils::CalUtils()
    : current_buffer_size_(0)
{
}

CalUtils::~CalUtils()
{
}

bool CalUtils::undistortFrame
(
    cv::Mat &frame,
    cv::Mat &cameraMatrix,
    cv::Mat &distCoeffs
)
{
    if (frame.empty() || cameraMatrix.empty() || distCoeffs.empty())
    {
        return false; // Invalid calibration matrices
    }
    cv::Mat undistorted;
    cv::undistort(frame, undistorted, cameraMatrix, distCoeffs);

    // Use clone() to ensure proper memory management
    frame = undistorted.clone();
    return true;
}

bool CalUtils::undistortFrameFisheye
(
    cv::Mat &frame,
    cv::Mat &cameraMatrix,
    cv::Mat &distCoeffs,
    cv::Mat &scaledCameraMatrix
)
{
    if (frame.empty())
    {
        return false;
    }

    if (cameraMatrix.empty() || distCoeffs.empty())
    {
        logger_->warning("Camera matrix or distortion coefficients are empty, cannot perform fisheye undistortion");
        return false; // Invalid calibration matrices
    }
    cv::Mat undistorted;
    if(scaledCameraMatrix.empty())
    {   // If no scaled camera matrix is provided, use the original camera
        // matrix for undistortion
        // This preserves the original focal lengths and principal point, which
        // is important for accurate undistortion
        cv::fisheye::undistortImage(frame, undistorted, cameraMatrix, distCoeffs, cameraMatrix);
    }
    else
    {   // If a scaled camera matrix is provided, use it for undistortion. This
        // allows for adjusting the focal lengths and principal point if needed.
        // This can be useful if we wants to apply a zoom effect or adjust the
        // field of view while still correcting for fisheye distortion
        cv::fisheye::undistortImage(frame, undistorted, cameraMatrix, distCoeffs, scaledCameraMatrix);
    }
    // Use clone() to ensure proper memory management
    frame = undistorted.clone();
    return true;
}

std::vector<cv::Point2f> &CalUtils::getLastCorners()
{
    return last_corners_;
}

std::vector<cv::Point3f> &CalUtils::getLastObjectPoints()
{
    return last_object_points_;
}

void CalUtils::setLastCorners(const std::vector<cv::Point2f> &corners)
{
    last_corners_ = corners;
}

void CalUtils::setLastObjectPoints(const std::vector<cv::Point3f> &object_points)
{
    last_object_points_ = object_points;
}

const bool CalUtils::hasLastCorners() const
{
    return !last_corners_.empty();
}

const bool CalUtils::hasLastObjectPoints() const
{
    return !last_object_points_.empty();
}

void CalUtils::clearLastCorners()
{
    last_corners_.clear();
}

void CalUtils::clearLastObjectPoints()
{
    last_object_points_.clear();
}

const std::vector<std::vector<cv::Point2f> > &CalUtils::getCalibrationCornersBuffer() const
{
    return calibration_corners_buffer_;
}

const std::vector<std::vector<cv::Point3f> > &CalUtils::getCalibrationObjectPointsBuffer() const
{
    return calibration_object_points_buffer_;
}

void CalUtils::addCalibrationData(const std::vector<cv::Point2f> &corners, const std::vector<cv::Point3f> &object_points)
{
    if(current_buffer_size_ < MAX_BUFFER_SIZE)
    {
        calibration_corners_buffer_.push_back(corners);
        calibration_object_points_buffer_.push_back(object_points);
        current_buffer_size_ = std::min(current_buffer_size_ + 1, MAX_BUFFER_SIZE);;
    }
}

void CalUtils::clearCalibrationCornersBuffer()
{
    calibration_corners_buffer_.clear();
}

void CalUtils::clearCalibrationObjectPointsBuffer()
{
    calibration_object_points_buffer_.clear();
}

void CalUtils::clearCalibrationBuffers()
{
    clearCalibrationCornersBuffer();
    clearCalibrationObjectPointsBuffer();
}

uint64_t CalUtils::getBufferSize() const
{
    return current_buffer_size_;
}
} // namespace PiTrac