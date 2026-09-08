#ifndef __PATTERN_DETECTOR_INTERFACE_H__
#define __PATTERN_DETECTOR_INTERFACE_H__

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include "Common/Utils/Calibration/CalibrationStruct.h"
#include "Common/Utils/Logging/GSLogger.h"
#include "Common/Camera/CameraStructs.h"

namespace PiTrac
{
class PatternDetector
{
  public:
    virtual ~PatternDetector() = default;

    /**
     * @brief Process a single image to find calibration points and evaluate quality of the image for calibration.
     * This is a pure virtual method that must be implemented by derived classes for specific calibration patterns (e.g., checkerboard, charuco).
     *
     * @param[in] image Input image to process (grayscale)
     * @param[out] debugImage Image with drawn corners and quality visualization for debugging
     * @param[out] corners Detected 2D image points of the calibration pattern
     * @param[out] objectPoints Corresponding 3D world points for the detected corners
     *
     * @return ImageQuality label indicating the quality of the image for calibration
     */
    virtual const ImageQuality processImage
    (
        cv::Mat &image,
        cv::Mat &debugImage,
        std::vector<cv::Point2f> &corners,
        std::vector<cv::Point3f> &objectPoints
    ) = 0;

    /**
     * @brief Set the expected dimensions of the calibration pattern (e.g., number of inner corners for checkerboard)
     * @param rows Number of rows in the pattern
     * @param cols Number of columns in the pattern
     */
    virtual void setDimensions(const uint32_t rows, const uint32_t cols)
    {
        checkerboardDimensions_ = {rows, cols};
    }

    /**
     * @brief Set the expected image size for calibration (used for quality evaluation)
     * @param width Image width in pixels
     * @param height Image height in pixels
     */
    virtual void setImageSize(const uint32_t width, const uint32_t height)
    {
        imageSize_ = {width, height};
    }

    virtual std::array<size_t, 2> getDimensions() const
    {
        return checkerboardDimensions_;
    }

    virtual std::array<size_t, 2> getImageSize() const
    {
        return imageSize_;
    }

  protected:

    /**
     * @brief Constructor for the PatternDetector interface. Initializes common members and sets up logging.
     * The specific processing function (standard vs fisheye) is determined by the calibration model
     *
     * @param model Calibration model (standard or fisheye) to determine processing method
     */
    PatternDetector(const CalibrationModel model)
        : calibrationModel_(model)
    {
        logger_ = GSLogger::getInstance();
    }

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
    virtual const ImageQuality screenImageQuality
    (
        cv::Mat &image,
        cv::Mat &debugImage,
        std::vector<cv::Point2f> &corners,
        std::vector<cv::Point3f> &objectPoints
    ) = 0;

    static std::string imageQualityToString(ImageQuality quality)
    {
        switch (quality)
        {
            case ImageQuality::REJECTED: return "REJECTED";
            case ImageQuality::GOOD: return "GOOD";
            case ImageQuality::VERY_GOOD: return "VERY GOOD";
            case ImageQuality::EXCELLENT: return "EXCELLENT";
            default: return "UNKNOWN";
        }
    }

    CalibrationModel calibrationModel_;
    cv::Size subPixWinSize_;
    std::array<size_t, 2> checkerboardDimensions_;
    std::array<size_t, 2> imageSize_;
    std::shared_ptr<GSLogger> logger_;
};
} // namespace PiTrac

#endif // __PATTERN_DETECTOR_INTERFACE_H__