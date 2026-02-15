#ifndef __CALIBRATE_DISTORTION_H__
#define __CALIBRATE_DISTORTION_H__

#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/aruco.hpp>
#include <opencv2/aruco/charuco.hpp>
#include <opencv2/objdetect/aruco_dictionary.hpp>
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "Common/Utils/Calibration/CalibrationStruct.h"
#include "Common/Utils/Logging/GSLogger.h"

// #define DEBUG_CALIBRATION

namespace PiTrac
{

/**
 * @brief Virtual class containing interface methods for camera distortion
 *calibration
 */
class CalibrateDistortion
{
  public:
    // Constants for calibration limits and thresholds
    static constexpr size_t MAX_CALIBRATION_IMAGES = 100;
    static constexpr size_t MIN_CALIBRATION_IMAGES = 3;
    static constexpr size_t MAX_CHECKERBOARD_ROWS = 20;
    static constexpr size_t MAX_CHECKERBOARD_COLS = 20;
    static constexpr size_t MIN_CHECKERBOARD_ROWS = 2;
    static constexpr size_t MIN_CHECKERBOARD_COLS = 2;
    static constexpr double MAX_REPROJECTION_ERROR = 5.0; // pixels
    static constexpr double REPROJECTION_EXCELLENT = 0.3; // pixels
    static constexpr double REPROJECTION_GOOD = 0.6; // pixels
    static constexpr double REPROJECTION_FAIR = 1.0; // pixels
    static constexpr double REPROJECTION_POOR = 2.0; // pixels
    static constexpr double CHECKERBOARD_COVERAGE_TOO_LOW = 0.2; // 20% of images with valid detections is too low
    static constexpr double CHECKERBOARD_COVERAGE_GOOD = 0.5; // 50% of images with valid detections is good
    static constexpr double CHECKERBOARD_COVERAGE_EXCELLENT = 0.8; // 80% of images with valid detections is excellent
    static constexpr double CHECKERBOARD_COVERAGE_TOO_HIGH = 0.95; // 95% of images with valid detections may indicate overfitting or lack of variety in calibration images
    static constexpr double SHARPNESS_THRESHOLD_LOW = 50.0; // Variance of Laplacian below this is considered too blurry
    static constexpr double SHARPNESS_THRESHOLD_HIGH = 100.0; // Variance of Laplacian above this is considered very sharp
    static constexpr double MIN_CORNER_CLUSTERING = 0.15; // Average distance of corners from center below this (normalized by image diagonal) is considered too clustered
    static constexpr double CORNER_CLUSTERING_GOOD = 0.20; // Average distance of corners from center above this (normalized by image diagonal) is considered good spread
    static constexpr double CORNER_CLUSTERING_EXCELLENT = 0.25; // Average distance of corners from center above this (normalized by image diagonal) is considered excellent spread

    enum class ImageQuality
    {
        REJECTED = 0,
        GOOD = 1,
        VERY_GOOD = 2,
        EXCELLENT = 3
    };

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

    virtual ImageQuality processImage
    (
        cv::Mat &image,
        cv::Mat &debugImage
    ) = 0; 

    size_t appendImage
    (
    );

    size_t popImage
    (
    );

    size_t clearLastImageData
    (
    );

    /**
     * @brief Clear all calibration images and reset count
     */
    void clearImages()
    {
        calibrationImages_.clear();
        lastImage_ = cv::Mat();
        lastObjectPoint_.clear();
        lastImagePoint_.clear();
        success_count_ = 0;
        fail_count_ = 0;
        numCalibrationImages_ = 0;
    }

    /**
     * @brief Set the calibration model to use
     * @param model Standard or Fisheye model
     */
    void setCalibrationModel(CalibrationModel model)
    {
        calibrationModel_ = model;
    }

    /**
     * @brief Get the current calibration model
     * @return Current calibration model
     */
    CalibrationModel getCalibrationModel() const
    {
        return calibrationModel_;
    }

    std::array<double, 5> getDistortionCoefficients() const
    {
        return distortionCoefficients_;
    }

    std::array<double, 4> getFisheyeDistortionCoefficients() const
    {
        return fisheyeDistortionCoefficients_;
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
     * @brief Check if calibration results are reasonable
     * @return True if calibration appears valid
     */
    bool isCalibrationValid() const;

    /**
     * @brief Perform distortion calibration
     * @return True if calibration was successful
     */
    virtual bool doDistortionCalibration();

    /**
     * @brief Calculate reprojection error for validation
     * @return RMS reprojection error in pixels (-1 if invalid)
     */
    virtual double getReprojectionError() const;

  protected:
    virtual bool findObjectAndImagePoints() = 0; // Pure virtual method to be implemented by derived classes  
    /**
     * @brief Checkerboard dimensions (number of inner corners per chessboard
     *row
     * and column)
     */
    size_t success_count_;
    size_t fail_count_;
    cv::Mat lastImage_;
    std::vector<cv::Point3f> lastObjectPoint_;
    std::vector<cv::Point2f> lastImagePoint_;
    std::vector<cv::Mat> calibrationImages_;
    std::array<uint32_t, 2> checkerboardDimensions_;
    std::array<double, 5> distortionCoefficients_; ///< Distortion coefficients
                                                   // (k1, k2, p1, p2, k3)
    std::array<double, 4> fisheyeDistortionCoefficients_; ///< Fisheye distortion coefficients (k1, k2, k3, k4)
    std::array<double, 4> cameraIntrinsics_;  ///< fx, fy, cx, cy
    CalibrationModel calibrationModel_ = CalibrationModel::STANDARD; ///< Current calibration model
    cv::Mat cameraMatrix_;                        ///< Camera matrix
    cv::Mat distCoeffs_;                         ///< Distortion coefficients
                                                 // matrix
    std::vector<std::vector<cv::Point3f> > objectPoints_; ///< 3D points in real
                                                          // world space
    std::vector<std::vector<cv::Point2f> > imagePoints_; ///< 2D points in image
                                                         // plane
    std::vector<cv::Mat> rvecs_;                    ///< Rotation vectors
    std::vector<cv::Mat> tvecs_;                    ///< Translation vectors
    std::vector<cv::Mat> drawnCalibrationImages_;   ///< Images with drawn
                                                    // corners
    std::vector<cv::Mat> undistortedCalibrationImages_; ///< Undistorted images for
                                                        // visualization
    size_t numCalibrationImages_;
    std::shared_ptr<GSLogger> logger_ = GSLogger::getInstance();
}; // End class CalibrateDistortion

class CheckerboardCalibration : public CalibrateDistortion
{
  public:

    CheckerboardCalibration();
    ~CheckerboardCalibration();
    ImageQuality processImage(cv::Mat &image, cv::Mat &debugImage) override;
  private:
    bool findObjectAndImagePoints() override; // Implement corner detection and point extraction
}; // End class CheckerboardCalibration

// class CharucoCalibration : public CalibrateDistortion
// {
//   public:

//     CharucoCalibration();
//     ~CharucoCalibration();

//     /**
//      * @brief Set the square and marker sizes for the CharuCo board
//      * @param squareLength Length of the checkerboard square side (in meters or any consistent unit)
//      * @param markerLength Length of the ArUco marker side (typically 75% of square length)
//      */
//     void setBoardSizes(float squareLength, float markerLength)
//     {
//         squareLength_ = squareLength;
//         markerLength_ = markerLength;
//     }

//     /**
//      * @brief Set the ArUco dictionary to use
//      * @param dictId ArUco dictionary ID (default: DICT_6X6_250)
//      */
//     void setArucoDictionary(int dictId)
//     {
//         arucoDictId_ = dictId;
//     }

//   private:
//     bool findObjectAndImagePoints() override; // Implement charuco corner detection and point extraction
    
//     float squareLength_ = 0.012f;  // 12mm squares
//     float markerLength_ = 0.009f;  // 9mm markers (75% of 12mm)
//     int arucoDictId_ = cv::aruco::DICT_6X6_1000;         // DICT_6X6_250
// }; // End class CharucoCalibration


} // End namespace PiTrac

#endif // __CALIBRATE_DISTORTION_H__