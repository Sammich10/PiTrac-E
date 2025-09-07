/**
 * Calibrate Distortions
 * 
 * This file is part of the PiTrac Software project.
 * 
 * calibrate_distortions:
 * 
 * This module provides functionality to calibrate camera distortions,
 * including lens distortion and perspective correction.
 */

#ifndef CALIBRATE_DISTORTIONS_HPP
#define CALIBRATE_DISTORTIONS_HPP

#include <opencv4/opencv2/opencv.hpp>
#include <opencv4/opencv2/core.hpp>
#include <opencv4/opencv2/imgproc.hpp>
#include <opencv4/opencv2/highgui.hpp>
#include <opencv4/opencv2/calib3d.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>

namespace pitrac {
class CalibrateDistortions {

CalibrateDistortions(
    cv::Size chessboardSize,
    float squareSize,
    cv::Size frameSize
);

void processImage
(
    const std::string& imagePathPattern
);

void calibrateCamera
(
    void
);

void undistortImage
(
    const std::string& inputImage,
    const std::string& outputImage
);

private:

cv::Size chessboardSize;
float squareSize;
cv::Size frameSize;
std::vector<std::vector<cv::Point3f>> objpoints;
std::vector<std::vector<cv::Point2f>> imgpoints;
cv::Mat cameraMatrix;
cv::Mat distCoeffs;
std::vector<cv::Mat> rvecs, tvecs;
std::vector<cv::Point3f> objp;

void prepareObjectPoints
(
    void
);

}; // class CalibrateDistortions

}  // namespace pitrac

#endif // CALIBRATE_DISTORTIONS_HPP