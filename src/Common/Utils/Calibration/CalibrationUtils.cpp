#include "Common/Utils/Calibration/CalibrationUtils.h"

namespace PiTrac
{

std::shared_ptr<GSLogger> CalUtils::logger_ = GSLogger::getInstance();

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
    {   // If no scaled camera matrix is provided, use the original camera matrix for undistortion
        // This preserves the original focal lengths and principal point, which is important for accurate undistortion
        cv::fisheye::undistortImage(frame, undistorted, cameraMatrix, distCoeffs, cameraMatrix);
    }
    else
    {   // If a scaled camera matrix is provided, use it for undistortion. This allows for adjusting the focal lengths and principal point if needed.
        // This can be useful if we wants to apply a zoom effect or adjust the field of view while still correcting for fisheye distortion
        cv::fisheye::undistortImage(frame, undistorted, cameraMatrix, distCoeffs, scaledCameraMatrix);
    }
    // Use clone() to ensure proper memory management
    frame = undistorted.clone();
    return true;
}

} // namespace PiTrac