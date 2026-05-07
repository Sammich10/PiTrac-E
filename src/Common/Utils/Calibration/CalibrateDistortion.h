#ifndef __CALIBRATE_DISTORTION_H__
#define __CALIBRATE_DISTORTION_H__

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <functional>

#include "Common/Utils/Calibration/CalibrationStruct.h"
#include "Common/Utils/Logging/GSLogger.h"
#include "Common/Camera/CameraStructs.h"
#include "Common/Utils/Calibration/Detectors/DetectorInterface.h"

// #define DEBUG_CALIBRATION

namespace PiTrac
{
/**
 * @brief Virtual class containing interface methods for camera distortion
 * calibration
 */
class CalibrateDistortion
{
  public:

    CalibrateDistortion
    (
        const CalibrationModel model = CalibrationModel::STANDARD
    );

    ~CalibrateDistortion();

    void setDimensions
    (
        const uint32_t rows,
        const uint32_t cols
    );

    void setImageSize
    (
        const uint32_t width,
        const uint32_t height
    );

    ImageQuality processImage
    (
        cv::Mat &images,
        cv::Mat &debugImages,
        std::vector<cv::Point2f> &corners,
        std::vector<cv::Point3f> &objectPoints
    );

    void reset();

    /**
     * @brief Perform distortion calibration
     * @return True if calibration was successful
     */
    virtual bool doDistortionCalibration
    (
        std::vector<std::vector<cv::Point2f> > &all_corners,
        std::vector<std::vector<cv::Point3f> > &all_object_points
    );

    /**
     * @brief Get the current calibration model
     * @return Current calibration model
     */
    CalibrationModel getCalibrationModel() const
    {
        return calibrationModel_;
    }

    /**
     * @brief Get the distortion coefficients from calibration
     * @param[in] camera Camera index
     * @return Distortion coefficients (k1, k2, p1, p2, k3 for standard; k1, k2, k3, k4 for fisheye)
     */
    DistortionCoefficients_Type getDistortionCoefficients() const
    {
        return DistortionCoefficients_Type(distCoeffs_, calibrationModel_);
    }

    /**
     * @brief Get the camera matrix from calibration
     * @param[in] camera Camera index
     * @return Camera matrix (3x3)
     */
    CameraIntrinsics_Type getCameraMatrix() const
    {
        return CameraIntrinsics_Type(cameraMatrix_);
    }

    double getReprojectionError() const
    {
        return reprojectionError_;
    }

  protected:

    const double doCalibrationStandard
    (
        std::vector<std::vector<cv::Point2f> > &all_corners,
        std::vector<std::vector<cv::Point3f> > &all_object_points
    );

    const double doCalibrationFisheye
    (
        std::vector<std::vector<cv::Point2f> > &all_corners,
        std::vector<std::vector<cv::Point3f> > &all_object_points
    );

    /**
     * @brief Calculate reprojection error for validation
     * @return RMS reprojection error in pixels (-1 if invalid)
     */
    virtual double getReprojectionError
    (
        std::vector<std::vector<cv::Point2f> > &all_corners,
        std::vector<std::vector<cv::Point3f> > &all_object_points,
        std::vector<cv::Mat> &rvecs,
        std::vector<cv::Mat> &tvecs
    ) const;

    std::function<double(std::vector<std::vector<cv::Point2f> > &, std::vector<std::vector<cv::Point3f> > &)> doCalibrationFunc_;

    CalibrationModel calibrationModel_;
    std::unique_ptr<PatternDetector> patternDetector_;
    cv::Mat cameraMatrix_;
    cv::Mat distCoeffs_;
    std::vector<cv::Mat> rvecs_;
    std::vector<cv::Mat> tvecs_;
    double reprojectionError_ = -1.0; // Initialize to invalid value
    std::shared_ptr<GSLogger> logger_;
}; // End class CalibrateDistortion

// class CharucoCalibration : public CalibrateDistortion
// {
//   public:

//     CharucoCalibration();
//     ~CharucoCalibration();

//     /**
//      * @brief Set the square and marker sizes for the CharuCo board
//      * @param squareLength Length of the checkerboard square side (in meters
// or any consistent unit)
//      * @param markerLength Length of the ArUco marker side (typically 75% of
// square length)
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
// detection and point extraction

//     float squareLength_ = 0.012f;  // 12mm squares
//     float markerLength_ = 0.009f;  // 9mm markers (75% of 12mm)
//     int arucoDictId_ = cv::aruco::DICT_6X6_1000;         // DICT_6X6_250
// }; // End class CharucoCalibration
} // End namespace PiTrac

#endif // __CALIBRATE_DISTORTION_H__