/**
 * @file ExampleExtrinsicCalibration.cpp
 * @brief Example demonstrating extrinsic calibration using known ball positions
 *
 * This example shows how to:
 * 1. Load pre-calibrated intrinsics and distortion from database
 * 2. Detect ball positions in camera image
 * 3. Use known 3D ball positions on calibration rig
 * 4. Compute camera pose (rotation and translation) using cv::solvePnP
 * 5. Save extrinsic calibration to database
 */
#if 0
#include "Common/Utils/Calibration/CalibrateExtrinsic.h"
#include "Common/Utils/Calibration/CalibrationData.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>

using namespace PiTrac;

/**
 * @brief Detect golf balls in image (simplified - replace with actual detection)
 * @param image Input image
 * @param ballCenters Output detected ball centers in image coordinates
 * @return True if balls detected successfully
 */
bool detectBalls(const cv::Mat &image, std::vector<cv::Point2d> &ballCenters)
{
    // TODO: Replace with actual ball detection algorithm
    // This is a placeholder that uses SimpleBlobDetector or HoughCircles

    cv::Mat gray;
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);

    // Use HoughCircles to detect circular objects (balls)
    std::vector<cv::Vec3f> circles;
    cv::HoughCircles(gray, circles, cv::HOUGH_GRADIENT, 1,
                     gray.rows / 8, // min distance between circles
                     100, 30,       // Canny threshold and accumulator threshold
                     5, 30);        // min and max radius

    ballCenters.clear();
    for (const auto &circle : circles)
    {
        ballCenters.push_back(cv::Point2d(circle[0], circle[1]));
    }

    return !ballCenters.empty();
}

/**
 * @brief Example: Calibrate camera extrinsics using ball calibration rig
 */
int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::cout << "Usage: " << argv[0] << " <camera_uuid> <calibration_image.jpg>" << std::endl;
        std::cout << "Example: " << argv[0] << " camera_uuid_123 calibration_rig.jpg" << std::endl;
        return -1;
    }

    std::string cameraUUID = argv[1];
    std::string imagePath = argv[2];

    // Load image
    cv::Mat image = cv::imread(imagePath);
    if (image.empty())
    {
        std::cerr << "Error: Could not load image " << imagePath << std::endl;
        return -1;
    }

    // Get calibration database instance
    auto calDB = CalibrationData::getInstance();

    // Load latest intrinsic calibration for this camera
    CalibrationEntry_Type latestEntry;
    if (!calDB->getLatestCalibrationEntry(cameraUUID, latestEntry))
    {
        std::cerr << "Error: No intrinsic calibration found for camera " << cameraUUID << std::endl;
        std::cerr << "Please run lens distortion calibration first!" << std::endl;
        return -1;
    }

    // Load intrinsics and distortion based on calibration type
    CameraIntrinsics_Type intrinsics;
    std::vector<double> distortion;
    bool isFisheye = (latestEntry.calibration_type == CalibrationModel::FISHEYE);

    if (isFisheye)
    {
        FisheyeDistortionCoefficients_Type fisheyeCoeffs;
        if (!calDB->getCalibrationEntryData(latestEntry, fisheyeCoeffs, intrinsics))
        {
            std::cerr << "Error: Could not load fisheye calibration data" << std::endl;
            return -1;
        }
        distortion = {fisheyeCoeffs.k1, fisheyeCoeffs.k2, fisheyeCoeffs.k3, fisheyeCoeffs.k4};
    }
    else
    {
        DistortionCoefficients_Type standardCoeffs;
        if (!calDB->getCalibrationEntryData(latestEntry, standardCoeffs, intrinsics))
        {
            std::cerr << "Error: Could not load standard calibration data" << std::endl;
            return -1;
        }
        distortion = {standardCoeffs.k1, standardCoeffs.k2, standardCoeffs.p1, standardCoeffs.p2, standardCoeffs.k3};
    }

    std::cout << "Loaded intrinsic calibration (CalibrationID: " << latestEntry.calibration_id << ")" << std::endl;
    std::cout << "  fx: " << intrinsics.focal_length_x << ", fy: " << intrinsics.focal_length_y << std::endl;
    std::cout << "  cx: " << intrinsics.principal_point_x << ", cy: " << intrinsics.principal_point_y << std::endl;

    // Define known 3D ball positions on calibration rig (in mm or meters)
    // TODO: Replace with actual measurements from your rig
    // Example: 4 balls in a square pattern at 200mm from camera, 100mm apart
    std::vector<cv::Point3d> worldPoints = {
        cv::Point3d(-50, -50, 200),  // Top-left ball
        cv::Point3d( 50, -50, 200),  // Top-right ball
        cv::Point3d( 50, 50, 200),   // Bottom-right ball
        cv::Point3d(-50, 50, 200)    // Bottom-left ball
    };

    std::cout << "\nKnown 3D ball positions on rig:" << std::endl;
    for (size_t i = 0; i < worldPoints.size(); i++)
    {
        std::cout << "  Ball " << i << ": [" << worldPoints[i].x << ", "
                  << worldPoints[i].y << ", " << worldPoints[i].z << "]" << std::endl;
    }

    // Detect balls in image
    std::vector<cv::Point2d> imagePoints;
    if (!detectBalls(image, imagePoints))
    {
        std::cerr << "Error: Could not detect balls in image" << std::endl;
        return -1;
    }

    std::cout << "\nDetected " << imagePoints.size() << " balls in image:" << std::endl;
    for (size_t i = 0; i < imagePoints.size(); i++)
    {
        std::cout << "  Ball " << i << ": [" << imagePoints[i].x << ", " << imagePoints[i].y << "]" << std::endl;
    }

    // Verify we have matching number of points
    if (imagePoints.size() != worldPoints.size())
    {
        std::cerr << "Error: Number of detected balls (" << imagePoints.size()
                  << ") doesn't match expected (" << worldPoints.size() << ")" << std::endl;
        return -1;
    }

    // Create extrinsic calibration object
    CalibrateExtrinsic extrinsicCal;
    extrinsicCal.setIntrinsics(intrinsics);
    extrinsicCal.setDistortion(distortion, isFisheye);

    // Add calibration point pairs
    for (size_t i = 0; i < worldPoints.size(); i++)
    {
        extrinsicCal.addCalibrationPoint(worldPoints[i], imagePoints[i]);
    }

    // Perform calibration
    std::cout << "\nPerforming extrinsic calibration..." << std::endl;
    auto quality = extrinsicCal.calibrate(true);  // Use iterative refinement

    if (quality == CalibrateExtrinsic::CalibrationQuality::FAILED)
    {
        std::cerr << "Error: Extrinsic calibration failed!" << std::endl;
        return -1;
    }

    // Get results
    CameraExtrinsics_Type extrinsics = extrinsicCal.getExtrinsics();

    std::cout << "\nExtrinsic Calibration Results:" << std::endl;
    std::cout << "  Rotation (rvec): [" << extrinsics.rvec[0] << ", "
              << extrinsics.rvec[1] << ", " << extrinsics.rvec[2] << "]" << std::endl;
    std::cout << "  Translation (tvec): [" << extrinsics.tvec[0] << ", "
              << extrinsics.tvec[1] << ", " << extrinsics.tvec[2] << "]" << std::endl;
    std::cout << "  Reprojection Error: " << extrinsics.reprojection_error << " pixels" << std::endl;
    std::cout << "  Number of Points: " << extrinsics.num_points << std::endl;

    // Save to database
    if (calDB->putExtrinsicCalibration(latestEntry.calibration_id, extrinsics))
    {
        std::cout << "\nExtrinsic calibration saved to database!" << std::endl;
    }
    else
    {
        std::cerr << "Warning: Could not save extrinsic calibration to database" << std::endl;
    }

    // Draw reprojection visualization
    cv::Mat debugImage = image.clone();
    extrinsicCal.drawReprojection(debugImage, worldPoints, imagePoints);

    // Save visualization
    std::string outputPath = "extrinsic_calibration_result.jpg";
    cv::imwrite(outputPath, debugImage);
    std::cout << "Reprojection visualization saved to: " << outputPath << std::endl;

    // Display (if running with GUI)
    cv::imshow("Extrinsic Calibration Result", debugImage);
    cv::waitKey(0);

    return 0;
}

#endif