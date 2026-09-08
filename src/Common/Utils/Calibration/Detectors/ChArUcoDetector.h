#ifndef __CHARUCO_DETECTOR_H__
#define __CHARUCO_DETECTOR_H__

#include "Common/Utils/Calibration/Detectors/DetectorInterface.h"
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/aruco.hpp>
#include <opencv2/aruco/charuco.hpp>
#include <opencv2/objdetect/aruco_dictionary.hpp>
#if 0
namespace PiTrac
{
class ChArUcoDetector : public PatternDetector
{
  public:
    ChArUcoDetector
    (
        const CalibrationModel model
    );
    virtual ~ChArUcoDetector() = default;

    const ImageQuality processImage
    (
        cv::Mat &image,
        cv::Mat &debugImage,
        std::vector<cv::Point2f> &corners,
        std::vector<cv::Point3f> &objectPoints
    )override;

  private:

    uint32_t squareLength_;
    uint32_t markerLength_;
    uint32_t arucoDictId_;

    /**
     * @brief Process a single image to find calibration points and evaluate quality of the image for calibration.
     * This is a pure virtual method that must be implemented by derived classes for specific calibration patterns (e.g., checkerboard, charuco).
     *
     * @param[in] image Input image to process (grayscale)
     * @param[out] debugImage Image with drawn corners and quality visualization for debugging
     * @param[out] corners Detected 2D image points of the checkerboard corners
     * @param[out] objectPoints Corresponding 3D world points for the detected corners
     *
     * @return ImageQuality label indicating the quality of the image for calibration
     */
    const ImageQuality processImageStandard
    (
        cv::Mat &image,
        cv::Mat &debugImage,
        std::vector<cv::Point2f> &corners,
        std::vector<cv::Point3f> &objectPoints
    )override;

    /**
     * @brief Process a single image using fisheye-specific corner detection and evaluation.
     * This is a pure virtual method that must be implemented by derived classes for specific calibration patterns (e.g., checkerboard, charuco) when using the fisheye model.
     *
     * @param[in] image Input image to process (grayscale)
     * @param[out] debugImage Image with drawn corners and quality visualization for debugging
     * @param[in] camera_num Camera index for logging and tracking
     *
     * @return ImageQuality label indicating the quality of the image for calibration
     */
    const ImageQuality processImageFisheye
    (
        cv::Mat &image,
        cv::Mat &debugImage,
        std::vector<cv::Point2f> &corners,
        std::vector<cv::Point3f> &objectPoints
    )override;

    /**
     * @brief Perform image quality screening based on criteria such as corner coverage, sharpness, and corner spread to evaluate the suitability of the image for calibration.
     *
     * @param[in] image Input image to evaluate
     * @param[in] debugImage Image to draw quality visualization on for debugging
     * @param[in] corners Detected 2D image points of the checkerboard corners
     * @param[in] objectPoints Corresponding 3D world points for the detected corners
     *
     * @return ImageQuality label indicating the quality of the image for calibration
     */
    const ImageQuality screenImageQuality
    (
        cv::Mat &image,
        cv::Mat &debugImage,
        std::vector<cv::Point2f> &corners,
        std::vector<cv::Point3f> &objectPoints
    )override;
};
}
#endif
#endif // __CHARUCO_DETECTOR_H__