#include "Common/Utils/Calibration/Detectors/ChArUcoDetector.h"
#if 0
namespace PiTrac
{
ChArUcoDetector::ChArUcoDetector(const CalibrationModel model)
    : PatternDetector(model)
{
}

const ImageQuality ChArUcoDetector::processImage(cv::Mat &image, cv::Mat &debugImage, std::vector<cv::Point2f> &corners, std::vector<cv::Point3f> &objectPoints)
{
    return processImageFunc_(image, debugImage, corners, objectPoints);
}

bool ChArUcoDetector::processImageStandard(cv::Mat &image, cv::Mat &debugImage, std::vector<cv::Point2f> &corners, std::vector<cv::Point3f> &objectPoints)
{
    logger_->info("  Square size: " + std::to_string(squareLength_) + "m");
    logger_->info("  Marker size: " + std::to_string(markerLength_) + "m");
    logger_->info("  Dictionary ID: " + std::to_string(arucoDictId_));

    // Create CharuCo board configuration
    // CharuCo board has (checkerboardDimensions + 1) squares per side
    // ArUco markers are placed in the white squares
    cv::Ptr<cv::aruco::Dictionary> dictionary =
        cv::makePtr<cv::aruco::Dictionary>(cv::aruco::getPredefinedDictionary(arucoDictId_));
    cv::Ptr<cv::aruco::CharucoBoard> charucoBoard =
        cv::makePtr<cv::aruco::CharucoBoard>(
            cv::Size(checkerboardDimensions_[0] + 1, checkerboardDimensions_[1] + 1), // Number of squares
            squareLength_, // Square side length
            markerLength_, // Marker side length
            *dictionary
            );

    cv::Ptr<cv::aruco::DetectorParameters> detectorParams = cv::makePtr<cv::aruco::DetectorParameters>();

    // Optimize detector parameters for fisheye if needed
    if (calibrationModel_ == CalibrationModel::FISHEYE)
    {
        detectorParams->adaptiveThreshWinSizeMin = 3;
        detectorParams->adaptiveThreshWinSizeMax = 23;
        detectorParams->adaptiveThreshWinSizeStep = 10;
        detectorParams->cornerRefinementMethod = cv::aruco::CORNER_REFINE_SUBPIX;
    }
    else
    {
        detectorParams->cornerRefinementMethod = cv::aruco::CORNER_REFINE_CONTOUR;
    }
    // Process each calibration image
    for(size_t i = 0; i < numCalibrationImages_; ++i)
    {
        // Validate image before processing
        if (calibrationImages_[i].empty())
        {
            logger_->error("Image " + std::to_string(i + 1) + " is empty, skipping");
            fail_count_++;
            continue;
        }

        std::vector<int> markerIds;
        std::vector<std::vector<cv::Point2f> > markerCorners;
        std::vector<cv::Point2f> charucoCorners;
        std::vector<int> charucoIds;

        // Detect ArUco markers
        cv::aruco::detectMarkers(calibrationImages_[i], dictionary, markerCorners, markerIds, detectorParams);

        logger_->info("Image " + std::to_string(i + 1) + ": Detected " + std::to_string(markerIds.size()) + " ArUco markers");

        // Log first few marker IDs to verify they match the expected range
        if (markerIds.size() > 0)
        {
            std::string idList = "";
            for (size_t j = 0; j < std::min(size_t(10), markerIds.size()); ++j)
            {
                idList += std::to_string(markerIds[j]) + " ";
            }
            logger_->info("  First marker IDs: " + idList + (markerIds.size() > 10 ? "..." : ""));
        }

        // Create debug image
        cv::Mat debugImage = calibrationImages_[i].clone();
        if (debugImage.channels() == 1)
        {
            cv::cvtColor(debugImage, debugImage, cv::COLOR_GRAY2BGR);
        }

        bool found = false;
        if (markerIds.size() > 0)
        {
            // Log marker ID range for debugging
            int minId = *std::min_element(markerIds.begin(), markerIds.end());
            int maxId = *std::max_element(markerIds.begin(), markerIds.end());
            logger_->info("  Marker ID range: " + std::to_string(minId) + " to " + std::to_string(maxId));

            // Interpolate CharuCo corners from detected markers
            int numInterpolated = cv::aruco::interpolateCornersCharuco(
                markerCorners
                , markerIds
                , calibrationImages_[i]
                , charucoBoard
                , charucoCorners
                , charucoIds
                );

            logger_->info("Image " + std::to_string(i + 1) + ": Interpolated " + std::to_string(charucoCorners.size()) + " CharuCo corners (return value: " + std::to_string(numInterpolated) + ")");

            // We need at least 4 corners for calibration
            if (charucoCorners.size() >= 4)
            {
                found = true;
                success_count_++;

                // Store the detected corners and their corresponding 3D
                object points
                imagePoints_.push_back(charucoCorners);
                // Get the 3D object points for the detected CharuCo corners
                std::vector<cv::Point3f> objPoints;
                std::vector<cv::Point3f> boardCorners = charucoBoard->getChessboardCorners();
                for (size_t j = 0; j < charucoIds.size(); ++j)
                {
                    cv::Point3f objPoint = boardCorners[charucoIds[j]];
                    objPoints.push_back(objPoint);
                }
                objectPoints_.push_back(objPoints);

                // Draw detected markers and CharuCo corners
                cv::aruco::drawDetectedMarkers(debugImage, markerCorners, markerIds);
                cv::aruco::drawDetectedCornersCharuco(debugImage, charucoCorners, charucoIds, cv::Scalar(0, 255, 0));

                cv::putText(debugImage, "FOUND: " + std::to_string(charucoCorners.size()) + " corners", cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 255, 0), 2);
            }
            else
            {
                fail_count_++;
                cv::aruco::drawDetectedMarkers(debugImage, markerCorners, markerIds);
                cv::putText(debugImage, "INSUFFICIENT CORNERS: " + std::to_string(charucoCorners.size()), cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 165, 255), 2);
                logger_->warning("Image " + std::to_string(i + 1) + ": Insufficient CharuCo corners (" + std::to_string(charucoCorners.size()) + " < 4)");
            }
        }
        else
        {
            fail_count_++;
            cv::putText(debugImage, "NO MARKERS DETECTED", cv::Point(10, 30),
                        cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 255), 2);
            logger_->warning("Image " + std::to_string(i + 1) + ": No ArUco markers detected");
        }

        // Add image index and board info
        cv::putText(debugImage, "Image " + std::to_string(i + 1), cv::Point(10, 70),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 2);
        cv::putText(debugImage, "CharuCo Board: " + std::to_string(checkerboardDimensions_[0] + 1) + "x" + std::to_string(checkerboardDimensions_[1] + 1),
                    cv::Point(10, 100), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);

        drawnCalibrationImages_.push_back(debugImage);

        // Save debug image with annotations
        std::string debugAnnotatedPath = "/tmp/debug_charuco_image_" +
                                         std::to_string(i + 1) + "_processed.jpg";
        cv::imwrite(debugAnnotatedPath, debugImage);
    }

    logger_->info("CharuCo detection complete: " +
                  std::to_string(success_count_) + " success, " + std::to_string(fail_count_) + " failed");
    return true;
}
}
#endif