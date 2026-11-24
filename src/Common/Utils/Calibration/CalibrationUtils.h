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
    static constexpr uint64_t MAX_BUFFER_SIZE = 100;

    CalUtils();

    ~CalUtils();

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

    std::vector<cv::Point2f> &getLastCorners();

    std::vector<cv::Point3f> &getLastObjectPoints();

    void setLastCorners
    (
        const std::vector<cv::Point2f> &corners
    );

    void setLastObjectPoints
    (
        const std::vector<cv::Point3f> &object_points
    );

    const bool hasLastCorners() const;

    const bool hasLastObjectPoints() const;

    void clearLastCorners();

    void clearLastObjectPoints();

    const std::vector<std::vector<cv::Point2f> > &getCalibrationCornersBuffer() const;

    const std::vector<std::vector<cv::Point3f> > &getCalibrationObjectPointsBuffer() const;

    void clearCalibrationCornersBuffer();

    void clearCalibrationObjectPointsBuffer();

    void clearCalibrationBuffers();

    uint64_t getBufferSize() const;

    void addCalibrationData
    (
        const std::vector<cv::Point2f> &corners,
        const std::vector<cv::Point3f> &object_points
    );

  private:
    std::vector<cv::Point2f> last_corners_;
    std::vector<cv::Point3f> last_object_points_;

    std::vector<std::vector<cv::Point2f> > calibration_corners_buffer_;
    std::vector<std::vector<cv::Point3f> > calibration_object_points_buffer_;

    uint64_t current_buffer_size_;

    static std::shared_ptr<GSLogger> logger_;
};
} // namespace PiTrac


#endif // __CALIBRATION_UTILS_H__