#ifndef __CALIBRATE_DISTORTION_H__
#define __CALIBRATE_DISTORTION_H__

#include <opencv2/opencv.hpp>
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "Common/Utils/Logging/GSLogger.h"

namespace PiTrac
{
/**
 * @brief Virtual class containing interface methods for camera distortion calibration
 */
class CalibrateDistortion
{
  public:
  // Constants for calibration limits and thresholds
  static constexpr size_t MAX_CALIBRATION_IMAGES = 100;
  static constexpr size_t MIN_CALIBRATION_IMAGES = 5;
  static constexpr size_t MAX_CHECKERBOARD_ROWS = 20;
  static constexpr size_t MAX_CHECKERBOARD_COLS = 20;
  static constexpr size_t MIN_CHECKERBOARD_ROWS = 2;
  static constexpr size_t MIN_CHECKERBOARD_COLS = 2;
  static constexpr double MAX_REPROJECTION_ERROR = 5.0; // pixels
  static constexpr double REPROJECTION_EXCELLENT = 0.3; // pixels
  static constexpr double REPROJECTION_GOOD = 0.6; // pixels
  static constexpr double REPROJECTION_FAIR = 1.0; // pixels
  static constexpr double REPROJECTION_POOR = 2.0; // pixels

  CalibrateDistortion();
  ~CalibrateDistortion();
  
  void setDimensions
  (
      const uint32_t rows,
      const uint32_t cols
  )
  {
      checkerboardDimensions_[0] = rows;
      checkerboardDimensions_[1] = cols;
  }

  void setImages
  (
    const std::vector<cv::Mat> &calibration_images
  );
  
  void clearImages()
  {
      calibrationImages_.clear();
      numCalibrationImages_ = 0;
  }

  
  std::array<double, 5> getDistortionCoefficients() const
  {
    return distortionCoefficients_;
  }

  std::array<double, 4> getCameraIntrinsics() const
  {
    return cameraIntrinsics_;
  }
    
  /**
   * @brief Get the camera matrix from calibration
   * @return Camera matrix (3x3)
   */
  cv::Mat getCameraMatrix() const
  {
    return cameraMatrix_.clone();
  }

  /**
   * @brief Get debug images with drawn chessboard corners
   * @return Vector of images with visualization
   */
  std::vector<cv::Mat> getDebugImages() const
  {
    return drawnCalibrationImages_;
  }

  /**
   * @brief Get quality metrics for the calibration
   * @return String with detailed quality information
   */
  std::string getCalibrationQuality() const;
  
  /**
   * @brief Check if calibration results are reasonable
   * @return True if calibration appears valid
   */
  bool isCalibrationValid() const;
  
  /**
   * @brief Perform distortion calibration
   * @return True if calibration was successful
   */
  virtual bool doDistortionCalibration() = 0;
    
  /**
   * @brief Calculate reprojection error for validation
   * @return RMS reprojection error in pixels (-1 if invalid)
   */
  virtual double getReprojectionError() const = 0;
  
  protected:
  /**
   * @brief Checkerboard dimensions (number of inner corners per chessboard row
   * and column)
   */
  std::vector<cv::Mat> calibrationImages_;
  std::array<uint32_t, 2> checkerboardDimensions_;
  std::array<double, 5> distortionCoefficients_; ///< Distortion coefficients (k1, k2, p1, p2, k3)
  std::array<double, 4> cameraIntrinsics_;    ///< fx, fy, cx, cy
  cv::Mat cameraMatrix_;                          ///< Camera matrix
  cv::Mat distCoeffs_;                           ///< Distortion coefficients matrix
  std::vector<std::vector<cv::Point3f>> objectPoints_; ///< 3D points in real world space
  std::vector<std::vector<cv::Point2f>> imagePoints_;  ///< 2D points in image plane
  std::vector<cv::Mat> rvecs_;                      ///< Rotation vectors
  std::vector<cv::Mat> tvecs_;                      ///< Translation vectors
  std::vector<cv::Mat> drawnCalibrationImages_;     ///< Images with drawn corners
  size_t numCalibrationImages_;
  std::shared_ptr<GSLogger> logger_ = GSLogger::getInstance();
}; // End class CalibrateDistortion

class CheckerboardCalibration : public CalibrateDistortion
{
  public:

  CheckerboardCalibration();
  ~CheckerboardCalibration();

  /**
   * @brief Perform distortion calibration using checkerboard images
   * @return True if calibration was successful
   */
  bool doDistortionCalibration() override;
    
  /**
   * @brief Calculate reprojection error for validation
   * @return RMS reprojection error in pixels (-1 if invalid)
   */
  double getReprojectionError() const override;
  
  /**
   * @brief Undistort an image using calibration results
   * @param distortedImage Input distorted image
   * @return Undistorted image
   */
  cv::Mat undistortImage(const cv::Mat& distortedImage) const;

  
  private:

}; // End class CheckerboardCalibration

} // End namespace PiTrac

#endif // __CALIBRATE_DISTORTION_H__