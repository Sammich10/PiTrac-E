#include "Common/Utils/Calibration/CalibrationUtils.h"

namespace PiTrac
{

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
    
    cv::Mat undistorted;
    cv::undistort(frame, undistorted, cameraMatrix, distCoeffs);
    frame = undistorted;  // Copy result back to original frame
    return true;
}

} // namespace PiTrac