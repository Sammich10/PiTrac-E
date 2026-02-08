#include "Common/Utils/Calibration/CalibrationUtils.h"

namespace PiTrac
{

cv::Mat CalUtils::undistortFrame
(
    const cv::Mat frame,
    cv::Mat &cameraMatrix,
    cv::Mat &distCoeffs
)
{
    if (frame.empty())
    {
        return cv::Mat();
    }
    cv::Mat undistorted;
    cv::undistort(frame, undistorted, cameraMatrix, distCoeffs);
    return undistorted;
}

} // namespace PiTrac