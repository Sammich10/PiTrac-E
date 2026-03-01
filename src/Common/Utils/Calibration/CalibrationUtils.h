#ifndef __CALIBRATION_UTILS_H__
#define __CALIBRATION_UTILS_H__

#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/calib3d.hpp>
#include "Common/Utils/Logging/GSLogger.h"

namespace PiTrac
{
class CalUtils
{
  public:

    static bool undistortFrame
    (
        cv::Mat &frame,
        cv::Mat &cameraMatrix,
        cv::Mat &distCoeffs
    );

    static bool undistortFrameFisheye
    (
        cv::Mat &frame,
        cv::Mat &cameraMatrix,
        cv::Mat &distCoeffs,
        cv::Mat &scaledCameraMatrix
    );

  private:
    static std::shared_ptr<GSLogger> logger_;
};
} // namespace PiTrac


#endif // __CALIBRATION_UTILS_H__