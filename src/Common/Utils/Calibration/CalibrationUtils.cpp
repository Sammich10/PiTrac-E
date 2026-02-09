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
    if (frame.empty())
    {
        return false;
    }
    
    if (cameraMatrix.empty() || distCoeffs.empty())
    {
        return false; // Invalid calibration matrices
    }

    try {
        cv::Mat undistorted;
        cv::undistort(frame, undistorted, cameraMatrix, distCoeffs);
        
        if (undistorted.empty()) {
            return false; // Undistortion failed
        }
        
        // Check if the result is all black/zero - but be more selective
        cv::Scalar meanVal = cv::mean(undistorted);
        double totalMean = meanVal[0];
        if (undistorted.channels() > 1) {
            totalMean += meanVal[1] + meanVal[2];
        }
        
        if (totalMean < 1.0) {  // Very dark image, likely an error
            return false;
        }
        
        // Ensure the undistorted frame has the same type and format as the original
        if (undistorted.type() != frame.type()) {
            return false;  // Type mismatch
        }
        
        // Use clone() to ensure proper memory management
        frame = undistorted.clone();
        return true;
    }
    catch (const cv::Exception& e) {
        // OpenCV exception during undistortion
        return false;
    }
}

bool CalUtils::undistortFrameFisheye
(
    cv::Mat &frame,
    cv::Mat &cameraMatrix,
    cv::Mat &distCoeffs
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

    try {
        cv::Mat undistorted;
        
        // Most basic fisheye undistortion - no fancy parameters
        cv::fisheye::undistortImage(frame, undistorted, cameraMatrix, distCoeffs);
        
        if (undistorted.empty()) {
            logger_->warning("Fisheye undistortion resulted in an empty image");
            return false;
        }
        
        // Just copy the result without any validation for now - let's see what we actually get
        logger_->info("Basic fisheye undistortion completed - original mean: " + std::to_string(cv::mean(frame)[0]) + ", result mean: " + std::to_string(cv::mean(undistorted)[0]) + ", size: " + std::to_string(undistorted.cols) + "x" + std::to_string(undistorted.rows));
        
        frame = undistorted.clone();
        return true;
    }
    catch (const cv::Exception& e) {
        // OpenCV exception during undistortion
        return false;
    }
}

} // namespace PiTrac