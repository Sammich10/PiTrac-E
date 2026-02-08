#ifndef __CALIBRATION_UTILS_H__
#define __CALIBRATION_UTILS_H__

#include <opencv2/opencv.hpp>
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
};
} // namespace PiTrac


#endif // __CALIBRATION_UTILS_H__