#include "Common/Utils/Calibration/CalibrateDistortion.h"

namespace PiTrac
{
CalibrateDistortion::CalibrateDistortion()
{
    // Initialize arrays to zero
    distortionCoefficients_.fill(0.0);
    fisheyeDistortionCoefficients_.fill(0.0);
    cameraIntrinsics_.fill(0.0);
}

CalibrateDistortion::~CalibrateDistortion()
{
}

void CalibrateDistortion::setImages(const std::vector<cv::Mat> &calibration_images)
{
    calibrationImages_.clear();
    logger_->info("Configuring calibration with " + std::to_string(calibration_images.size()) + " input images");

    // Convert images to grayscale if necessary and validate
    for (size_t i = 0; i < calibration_images.size(); ++i)
    {
        const cv::Mat &img = calibration_images[i];

        // Skip empty images
        if (img.empty())
        {
            logger_->warning("Input image " + std::to_string(i) + " is empty, skipping");
            continue;
        }

        if (img.channels() == 3)
        {
            cv::Mat gray;
            cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
            calibrationImages_.push_back(gray);
        }
        else if (img.channels() == 1)
        {
            calibrationImages_.push_back(img.clone()); // Use clone() to ensure
                                                       // independent copy
        }
        else
        {
            logger_->warning("Image " + std::to_string(i) + " has unsupported channel count: " +
                             std::to_string(img.channels()) + ", skipping");
        }
    }

    numCalibrationImages_ = calibrationImages_.size();
    logger_->info("Final calibration image count: " + std::to_string(numCalibrationImages_));
}

bool CalibrateDistortion::isCalibrationValid() const
{
    if (cameraMatrix_.empty() || objectPoints_.size() < 3)
    {
        return false;
    }

    double rms = getReprojectionError();
    if (rms < 0 || rms > 2.0)
    {
        return false;  // RMS too high or invalid
    }

    double fx = cameraMatrix_.at<double>(0, 0);
    double fy = cameraMatrix_.at<double>(1, 1);
    double aspectRatio = fx / fy;

    if (aspectRatio < 0.8 || aspectRatio > 1.2)
    {
        return false;  // Aspect ratio too far from 1.0
    }

    return true;
}


double CalibrateDistortion::getReprojectionError() const
{
    if (objectPoints_.empty() || imagePoints_.empty() || cameraMatrix_.empty())
    {
        return -1.0; // Invalid
    }

    std::vector<cv::Point2f> imagePoints2;
    double totalError = 0;
    int totalPoints = 0;

    for (size_t i = 0; i < objectPoints_.size(); i++)
    {
        switch(calibrationModel_)
        {
            case CalibrationModel::FISHEYE:
            {
                cv::fisheye::projectPoints(objectPoints_[i], imagePoints2, rvecs_[i], tvecs_[i], 
                                           cameraMatrix_, distCoeffs_);
                break;
            }
            case CalibrationModel::STANDARD:
            default:
            {
                cv::projectPoints(objectPoints_[i], rvecs_[i], tvecs_[i], cameraMatrix_,
                                  distCoeffs_, imagePoints2);
                break;
            }
        }
        double err = cv::norm(imagePoints_[i], imagePoints2, cv::NORM_L2);
        totalError += err * err;
        totalPoints += static_cast<int>(objectPoints_[i].size());
    }

    return std::sqrt(totalError / totalPoints);
}

bool CalibrateDistortion::doDistortionCalibration()
{
    // Clear previous results
    imagePoints_.clear();
    objectPoints_.clear();
    drawnCalibrationImages_.clear();

    success_count_ = 0;
    fail_count_ = 0;

    findObjectAndImagePoints();

    if (success_count_ < 3)
    {
        logger_->error("Insufficient valid images for calibration (need at least 3, got " +
                       std::to_string(success_count_) + ")");
        return false;
    }
    // Perform camera calibration with improved flags
    logger_->info("Running camera calibration with " + std::to_string(success_count_) + " valid images...");
    cv::Size imageSize = calibrationImages_[0].size();
    double rms = 0.0;
    if(calibrationModel_ == CalibrationModel::FISHEYE)
    {
        logger_->info("Using fisheye calibration model");
        // Fisheye calibration flags
        int fisheyeFlags = cv::fisheye::CALIB_RECOMPUTE_EXTRINSIC | cv::fisheye::CALIB_FIX_SKEW;
        cv::Vec4d distCoeffs4; // Temporary storage for fisheye distortion coefficients
        rms = cv::fisheye::calibrate(
            objectPoints_,
            imagePoints_,
            imageSize,
            cameraMatrix_,
            distCoeffs4,
            rvecs_,
            tvecs_,
            fisheyeFlags,
            cv::TermCriteria(3, 20, 1e-6)
        );
       // Convert fisheye distortion coefficients to array (k1, k2, k3, k4)
        for (int i = 0; i < 4; i++) {
            fisheyeDistortionCoefficients_[i] = distCoeffs4[i];
        }
        distCoeffs_ = cv::Mat(distCoeffs4).reshape(1, 4); // Store in distCoeffs_ for consistency
    }
    else
    {
        logger_->info("Using standard pinhole calibration model");
        // Use flags that allow better distortion coefficient estimation
        // int calibrationFlags = cv::CALIB_RATIONAL_MODEL;
        int calibrationFlags = 0; // No special flags for standard model, can be adjusted as needed
        rms = cv::calibrateCamera(
            objectPoints_,
            imagePoints_,
            imageSize,
            cameraMatrix_,
            distCoeffs_,
            rvecs_,
            tvecs_,
            calibrationFlags,
            cv::TermCriteria(cv::TermCriteria::COUNT + cv::TermCriteria::EPS, 100, 1e-6)
        );
        // Convert distortion coefficients to array, expecting column vector
        if(distCoeffs_.cols >= 5)
        {
            for(uint32_t i = 0; i < 5; ++i)
            {
                distortionCoefficients_[i] = distCoeffs_.at<double>(0, i);
            }
        }
        else
        {
            logger_->warning("Unexpected distortion coefficients size: " + std::to_string(distCoeffs_.rows) + "x" + std::to_string(distCoeffs_.cols));
        }
    }
    // Convert camera matrix to array, expecting 3x3 matrix
    if(cameraMatrix_.rows == 3 && cameraMatrix_.cols == 3)
    {
        cameraIntrinsics_[0] = cameraMatrix_.at<double>(0, 0); // fx
        cameraIntrinsics_[1] = cameraMatrix_.at<double>(1, 1); // fy
        cameraIntrinsics_[2] = cameraMatrix_.at<double>(0, 2); // cx
        cameraIntrinsics_[3] = cameraMatrix_.at<double>(1, 2); // cy
    }
    else
    {
        logger_->warning("Unexpected camera matrix size: " + std::to_string(cameraMatrix_.rows) + "x" + std::to_string(cameraMatrix_.cols));
    }

    logger_->info("Camera calibration complete!");
    logger_->info("RMS reprojection error: " + std::to_string(rms) + " pixels");
    logger_->info("Camera matrix:");
    logger_->info("  fx: " + std::to_string(cameraIntrinsics_[0]));
    logger_->info("  fy: " + std::to_string(cameraIntrinsics_[1]));
    logger_->info("  cx: " + std::to_string(cameraIntrinsics_[2]));
    logger_->info("  cy: " + std::to_string(cameraIntrinsics_[3]));
    if(calibrationModel_ == CalibrationModel::FISHEYE)
    {
        logger_->info("Fisheye distortion coefficients:");
        logger_->info("  k1: " + std::to_string(fisheyeDistortionCoefficients_[0]));
        logger_->info("  k2: " + std::to_string(fisheyeDistortionCoefficients_[1]));
        logger_->info("  k3: " + std::to_string(fisheyeDistortionCoefficients_[2]));
        logger_->info("  k4: " + std::to_string(fisheyeDistortionCoefficients_[3]));
    }
    else
    {
        logger_->info("Standard distortion coefficients:");
        logger_->info("  k1: " + std::to_string(distortionCoefficients_[0]));
        logger_->info("  k2: " + std::to_string(distortionCoefficients_[1]));
        logger_->info("  p1: " + std::to_string(distortionCoefficients_[2]));
        logger_->info("  p2: " + std::to_string(distortionCoefficients_[3]));
        logger_->info("  k3: " + std::to_string(distortionCoefficients_[4]));
    }
    logger_->info("Used " + std::to_string(success_count_) + " of " + std::to_string(numCalibrationImages_) + " valid images for calibration");

    return (rms < REPROJECTION_FAIR); // Consider calibration successful if RMS error < 1 pixel
}

CheckerboardCalibration::CheckerboardCalibration()
    : CalibrateDistortion()
{
}

CheckerboardCalibration::~CheckerboardCalibration()
{
}

bool CheckerboardCalibration::findObjectAndImagePoints()
{
    
    logger_->info("Starting checkerboard calibration with " + std::to_string(numCalibrationImages_) + " images");
    logger_->info("Checkerboard size: " + std::to_string(checkerboardDimensions_[0]) + "x" + std::to_string(checkerboardDimensions_[1]));

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
        // Preprocess image for better detection
        cv::Mat processedImage;
        // Apply gentle gaussian blur to reduce noise
        cv::GaussianBlur(calibrationImages_[i], processedImage, cv::Size(3, 3), 0.5);
        // Enhance contrast using CLAHE (Contrast Limited Adaptive Histogram
        // Equalization)
        cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
        clahe->apply(processedImage, processedImage);
        std::vector<cv::Point2f> corners;
        // Try multiple detection strategies for better success rate
        bool found = false;
        // Strategy 1: Standard detection with preprocessing
        found = cv::findChessboardCorners
            (
                processedImage,
                cv::Size(checkerboardDimensions_[0], checkerboardDimensions_[1]),
                corners,
                cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE | cv::CALIB_CB_FILTER_QUADS
            );
        // Strategy 2: If first attempt fails, try with original image and
        // different flags
        if (!found)
        {
            logger_->info("Attempt with processed image failed, trying with original image...");
            found = cv::findChessboardCorners
                (
                    calibrationImages_[i],
                    cv::Size(checkerboardDimensions_[0], checkerboardDimensions_[1]),
                    corners,
                    cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE
                );
        }
        // Strategy 3: Try with morphological operations
        if (!found)
        {
            logger_->info("Second attempt failed, trying with morphological processing...");
            cv::Mat morphProcessed;
            cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
            cv::morphologyEx(calibrationImages_[i], morphProcessed, cv::MORPH_CLOSE, kernel);
            found = cv::findChessboardCorners
                    (
                        morphProcessed,
                        cv::Size(checkerboardDimensions_[0], checkerboardDimensions_[1]),
                        corners,
                        cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_FILTER_QUADS
                    );
        }
        logger_->info("Chessboard detection result for image " + std::to_string(i + 1) + ": " + std::string(found ? "SUCCESS" : "FAILED") +
                      " (found " + std::to_string(corners.size()) + " corners)");
        // Create debug image (convert grayscale back to color for
        // visualization)
        cv::Mat debugImage = calibrationImages_[i].clone();
        if (debugImage.channels() == 1)
        {
            cv::cvtColor(debugImage, debugImage, cv::COLOR_GRAY2BGR);
        }
        if (found)
        {
            success_count_++;
            // Refine corner locations for sub-pixel accuracy
            cv::cornerSubPix
            (
                calibrationImages_[i],
                corners,
                cv::Size(11, 11),
                cv::Size(-1, -1),
                cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 30, 0.001)
            );
            imagePoints_.push_back(corners);
            // Generate 3D object points for this chessboard
            std::vector<cv::Point3f> objp;
            for (uint32_t r = 0; r < checkerboardDimensions_[1]; ++r)
            {
                for (uint32_t c = 0; c < checkerboardDimensions_[0]; ++c)
                {
                    objp.emplace_back(c, r, 0.0f);
                }
            }
            objectPoints_.push_back(objp);
            // Draw corners on debug image
            cv::drawChessboardCorners(debugImage,
                                      cv::Size(checkerboardDimensions_[0], checkerboardDimensions_[1]),
                                      corners, found);
            // Add success indicator
            cv::putText(debugImage, "FOUND", cv::Point(10, 30),
                        cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 255, 0), 2);
        }
        else
        {
            fail_count_++;
            // Add failure indicator
            cv::putText(debugImage, "NOT FOUND", cv::Point(10, 30),
                        cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 255), 2);
            logger_->warning("Image " + std::to_string(i + 1) + ": Chessboard corners not found");
        }

        // Add image index and checkerboard info
        cv::putText(debugImage, "Image " + std::to_string(i + 1), cv::Point(10, 70),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 2);
        cv::putText(debugImage, "Board: " + std::to_string(checkerboardDimensions_[0]) + "x" + std::to_string(checkerboardDimensions_[1]),
                    cv::Point(10, 100), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);

        drawnCalibrationImages_.push_back(debugImage);
        // Save debug image with annotations
        // std::string debugAnnotatedPath = "/tmp/debug_cal_image_" +
        // std::to_string(i + 1) + "_processed.jpg";
        // cv::imwrite(debugAnnotatedPath, debugImage);
    }

    logger_->info("Corner detection complete: " + std::to_string(success_count_) + " success, " +
                  std::to_string(fail_count_) + " failed");
    return true;
}

} // End namespace PiTrac