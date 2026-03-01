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

size_t CalibrateDistortion::appendImage()
{
    if (lastImage_.empty())
    {
        logger_->warning("Attempted to append empty image, ignoring");
        return numCalibrationImages_;
    }

    if (lastImage_.channels() == 3)
    {
        cv::Mat gray;
        cv::cvtColor(lastImage_, gray, cv::COLOR_BGR2GRAY);
        calibrationImages_.push_back(gray);
    }
    else if (lastImage_.channels() == 1)
    {
        calibrationImages_.push_back(lastImage_.clone());
        objectPoints_.push_back(std::vector<cv::Point3f>{lastObjectPoint_});
        imagePoints_.push_back(std::vector<cv::Point2f>{lastImagePoint_});
    }
    else
    {
        logger_->warning("Attempted to append image with unsupported channel count: " +
                         std::to_string(lastImage_.channels()) + ", ignoring");
        return calibrationImages_.size();
    }

    numCalibrationImages_ = calibrationImages_.size();
    logger_->info("Appended image to calibration set. Total images: " + std::to_string(numCalibrationImages_));
    return numCalibrationImages_;
}

size_t CalibrateDistortion::popImage()
{
    if (calibrationImages_.empty())
    {
        logger_->warning("Attempted to pop image from empty calibration set, ignoring");
        return 0;
    }
    calibrationImages_.pop_back();
    numCalibrationImages_ = calibrationImages_.size();
    logger_->info("Popped last image from calibration set. Total images: " + std::to_string(numCalibrationImages_));
    return numCalibrationImages_;
}

size_t CalibrateDistortion::clearLastImageData()
{
    lastImage_ = cv::Mat();
    lastObjectPoint_.clear();
    lastImagePoint_.clear();
    return 0;
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
    rvecs_.clear();
    tvecs_.clear();

    if(calibrationImages_.size() < MIN_CALIBRATION_IMAGES)
    {
        logger_->error("Not enough valid calibration images to perform calibration. Need at least " + std::to_string(MIN_CALIBRATION_IMAGES) + ", have " + std::to_string(calibrationImages_.size()));
        return false;
    }

    // Perform camera calibration with improved flags
    logger_->info("Running camera calibration with " + std::to_string(calibrationImages_.size()) + " valid images...");
    cv::Size imageSize = calibrationImages_[0].size();
    double rms = 0.0;
    if(calibrationModel_ == CalibrationModel::FISHEYE)
    {
        logger_->info("Using fisheye calibration model");

        // Initialize camera matrix with reasonable estimate based on image size
        // This is critical for fisheye calibration to converge properly
        cameraMatrix_ = cv::Mat::eye(3, 3, CV_64F);
        double fx_estimate = imageSize.width * 0.6;  // Typical for fisheye:
                                                     // focal length ~= 0.6 *
                                                     // width
        double fy_estimate = imageSize.width * 0.6;
        double cx_estimate = imageSize.width / 2.0;
        double cy_estimate = imageSize.height / 2.0;

        cameraMatrix_.at<double>(0, 0) = fx_estimate;
        cameraMatrix_.at<double>(1, 1) = fy_estimate;
        cameraMatrix_.at<double>(0, 2) = cx_estimate;
        cameraMatrix_.at<double>(1, 2) = cy_estimate;

        logger_->info("Initial camera matrix estimate:");
        logger_->info("  fx: " + std::to_string(fx_estimate) + ", fy: " + std::to_string(fy_estimate));
        logger_->info("  cx: " + std::to_string(cx_estimate) + ", cy: " + std::to_string(cy_estimate));

        // Fisheye calibration flags:
        // CALIB_RECOMPUTE_EXTRINSIC: Recompute extrinsic params after each
        // iteration (essential for convergence)
        // CALIB_FIX_SKEW: Fix skew to 0 (assume rectangular pixels)
        // Note: CALIB_CHECK_COND removed - it's too strict and causes failures
        // with valid calibration data
        int calibrationFlags = cv::fisheye::CALIB_RECOMPUTE_EXTRINSIC |
                               cv::fisheye::CALIB_FIX_SKEW;

        cv::Vec4d distCoeffs4; // Temporary storage for fisheye distortion
                               // coefficients

        rms = cv::fisheye::calibrate(
            objectPoints_,
            imagePoints_,
            imageSize,
            cameraMatrix_,
            distCoeffs4,
            rvecs_,
            tvecs_,
            calibrationFlags,
            cv::TermCriteria(cv::TermCriteria::COUNT + cv::TermCriteria::EPS, 100, 1e-6)
            );

        logger_->info("After fisheye calibration - cameraMatrix:\n  fx=" + std::to_string(cameraMatrix_.at<double>(0, 0)) +
                      ", fy=" + std::to_string(cameraMatrix_.at<double>(1, 1)) +
                      ", cx=" + std::to_string(cameraMatrix_.at<double>(0, 2)) +
                      ", cy=" + std::to_string(cameraMatrix_.at<double>(1, 2)));
        logger_->info("Distortion coefficients: [" + std::to_string(distCoeffs4[0]) + ", " + std::to_string(distCoeffs4[1]) + ", " +
                      std::to_string(distCoeffs4[2]) + ", " + std::to_string(distCoeffs4[3]) + "]");

        // Convert fisheye distortion coefficients to array (k1, k2, k3, k4)
        for (int i = 0; i < 4; i++)
        {
            fisheyeDistortionCoefficients_[i] = distCoeffs4[i];
        }
        distCoeffs_ = cv::Mat(distCoeffs4).reshape(1, 4); // Store in
                                                          // distCoeffs_ for
                                                          // consistency

        logger_->info("Stored to member arrays - fisheyeDistortionCoefficients_: [" +
                      std::to_string(fisheyeDistortionCoefficients_[0]) + ", " +
                      std::to_string(fisheyeDistortionCoefficients_[1]) + ", " +
                      std::to_string(fisheyeDistortionCoefficients_[2]) + ", " +
                      std::to_string(fisheyeDistortionCoefficients_[3]) + "]");
    }
    else
    {
        logger_->info("Using standard pinhole calibration model");
        // Use flags that allow better distortion coefficient estimation
        // int calibrationFlags = cv::CALIB_RATIONAL_MODEL;
        int calibrationFlags = 0; // No special flags for standard model, can be
                                  // adjusted as needed
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
        for(uint32_t i = 0; i < 5; ++i)
        {
            distortionCoefficients_[i] = distCoeffs_.at<double>(0, i);
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
    #ifdef DEBUG_CALIBRATION
    // Apply the calibration to each image and store debug visualizations
    for (size_t i = 0; i < calibrationImages_.size(); ++i)
    {
        cv::Mat undistorted;
        if(calibrationModel_ == CalibrationModel::FISHEYE)
        {
            // For fisheye undistortion, use the calibrated camera matrix
            // directly as the new camera matrix
            // This preserves the correct focal lengths and principal point
            cv::fisheye::undistortImage(calibrationImages_[i], undistorted, cameraMatrix_, distCoeffs_, cameraMatrix_);
        }
        else
        {
            cv::undistort(calibrationImages_[i], undistorted, cameraMatrix_, distCoeffs_);
        }
        undistortedCalibrationImages_.push_back(undistorted);
        std::string filename = "/tmp/calibration_debug_image_" + std::to_string(i + 1) + ".jpg";
        cv::imwrite(filename, drawnCalibrationImages_[i]);
        filename = "/tmp/calibration_undistorted_image_" + std::to_string(i + 1) + ".jpg";
        cv::imwrite(filename, undistorted);
        logger_->info("Saved undistorted calibration image " + std::to_string(i + 1) + " to " + filename);
    }
    #endif

    return (rms < REPROJECTION_FAIR); // Consider calibration successful if RMS
                                      // error < 1 pixel
}

CheckerboardCalibration::CheckerboardCalibration()
    : CalibrateDistortion()
{
}

CheckerboardCalibration::~CheckerboardCalibration()
{
}

CalibrateDistortion::ImageQuality CheckerboardCalibration::processImage(cv::Mat &image, cv::Mat &debugImage)
{
    // Validate image before processing
    if (image.empty())
    {
        logger_->error("Image is empty, skipping");
        fail_count_++;
        return ImageQuality::REJECTED;
    }
    debugImage = image.clone(); // Initialize debug image
    // Convert to grayscale if necessary
    if (image.channels() == 3)
    {
        cv::Mat gray;
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        image = gray.clone();
    }
    std::vector<cv::Point2f> corners;
    // Attempt to find chessboard corners using OpenCV's built-in function with
    // adaptive thresholding and normalization
    bool found = cv::findChessboardCorners(
        image,
        cv::Size(checkerboardDimensions_[0], checkerboardDimensions_[1]),
        corners,
        cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE
        );
    if(!found)
    {
        logger_->info("Initial chessboard detection failed for image, trying with gentle blur...");
        // Try with gentle blur to reduce noise
        cv::Mat blurred;
        cv::GaussianBlur(image, blurred, cv::Size(3, 3), 0.5);

        found = cv::findChessboardCorners(
            blurred,
            cv::Size(checkerboardDimensions_[0], checkerboardDimensions_[1]),
            corners,
            cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE
            );
    }
    if(!found && calibrationModel_ != CalibrationModel::FISHEYE)
    {
        logger_->info("Chessboard detection with blur failed, trying with CLAHE...");
        // Try with CLAHE to improve contrast (only for standard model, as it
        // can hurt fisheye detection)
        cv::Mat processedImage;
        cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
        clahe->apply(image, processedImage);

        found = cv::findChessboardCorners
                (
            processedImage,
            cv::Size(checkerboardDimensions_[0], checkerboardDimensions_[1]),
            corners,
            cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE | cv::CALIB_CB_FILTER_QUADS
                );
    }
    if(!found)
    {
        logger_->warning("Chessboard detection failed for image after all attempts");
        // Add failure indicator
        cv::putText(debugImage, "NOT FOUND", cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 255), 2);
        return ImageQuality::REJECTED;
    }

    // Draw corners on debug image
    cv::drawChessboardCorners(debugImage, cv::Size(checkerboardDimensions_[0], checkerboardDimensions_[1]), corners, found);
    logger_->info("Chessboard detection result for image " + std::to_string(numCalibrationImages_ + 1) + ": " + std::string(found ? "SUCCESS" : "FAILED") +
                  " (found " + std::to_string(corners.size()) + " corners)");
    // Perform some image quality screening to reject images that are unlikely
    // to yield good calibration results, even if
    //  corners were detected. This helps ensure we only use high-quality images
    // for calibration and avoid skewing results
    // with poor data.

    // Quality screen 1: Coverage - Ensure corners are well-distributed across
    // the image
    cv::Rect bbox = cv::boundingRect(corners);
    double coverage = (bbox.width * bbox.height) / static_cast<double>(image.cols * image.rows);
    // Quality screen 2: Sharpness - Use Laplacian variance to detect blur
    cv::Mat laplacian;
    cv::Laplacian(image(bbox), laplacian, CV_64F);
    cv::Scalar mean, stddev;
    cv::meanStdDev(laplacian, mean, stddev);
    double sharpness = stddev[0] * stddev[0];
    // Quality screen 3: Corner spread - Compute average distance of corners
    // from their center of mass
    cv::Point2f centerOfMass(0, 0);
    for (const auto &corner : corners)
    {
        centerOfMass += corner;
    }
    centerOfMass *= (1.0f / corners.size());
    double avgDistFromCenter = 0.0;
    for (const auto &corner : corners)
    {
        avgDistFromCenter += cv::norm(corner - centerOfMass);
    }
    avgDistFromCenter /= corners.size();
    double imageDiagonal = std::sqrt(image.cols * image.cols + image.rows * image.rows);
    double normalizedSpread = avgDistFromCenter / imageDiagonal;
    std::vector<std::string> rejectReasons;
    bool accept = true;

    // Coverage screen: ensure that the detected corners cover a reasonable
    // portion of the image (not too small or too large)
    if(coverage < CHECKERBOARD_COVERAGE_TOO_LOW)
    {
        accept = false;
        rejectReasons.push_back("COVERAGE TOO LOW (" + std::to_string(static_cast<int>(coverage * 100)) + "%)");
    }
    else if(coverage > CHECKERBOARD_COVERAGE_TOO_HIGH)
    {
        accept = false;
        rejectReasons.push_back("COVERAGE TOO HIGH (" + std::to_string(static_cast<int>(coverage * 100)) + "%)");
    }
    // Sharpness screen: ensure that the image isn't too blurry (low variance of
    // Laplacian) or unrealistically sharp (high variance, likely noise)
    if(sharpness < SHARPNESS_THRESHOLD_LOW)
    {
        accept = false;
        rejectReasons.push_back("TOO BLURRY (sharpness: " + std::to_string(static_cast<int>(sharpness)) + ")");
    }
    // Spread screen: Ensure the corner points are not too clustered together
    if(normalizedSpread < MIN_CORNER_CLUSTERING)
    {
        accept = false;
        rejectReasons.push_back("CLUSTERED (spread: " + std::to_string(static_cast<int>(normalizedSpread * 100)) + "%)");
    }
    // If any quality screen failed, reject the image and log all reasons
    if(!accept)
    {
        logger_->warning("Image " + std::to_string(numCalibrationImages_ + 1) + " rejected: " + std::to_string(rejectReasons.size()) + " quality issues:");
        for(const auto &reason : rejectReasons)
        {
            logger_->warning("  - " + reason);
            cv::putText(debugImage, reason, cv::Point(10, 30 + 30 * rejectReasons.size()), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 165, 255), 2);
        }
        return ImageQuality::REJECTED;
    }
    lastImage_ = image.clone();
    // Image passed all minimum quality screens - categorize quality based on
    // how well it meets criteria
    ImageQuality qualityLabel;
    cv::Scalar qualityColor;
    if (coverage >= CHECKERBOARD_COVERAGE_GOOD && sharpness > SHARPNESS_THRESHOLD_HIGH && normalizedSpread > CORNER_CLUSTERING_EXCELLENT)
    {
        qualityLabel = ImageQuality::EXCELLENT;
        qualityColor = cv::Scalar(0, 255, 0); // Green
    }
    else if (coverage >= CHECKERBOARD_COVERAGE_TOO_LOW && sharpness > SHARPNESS_THRESHOLD_LOW && normalizedSpread > CORNER_CLUSTERING_GOOD)
    {
        qualityLabel = ImageQuality::VERY_GOOD;
        qualityColor = cv::Scalar(0, 200, 200); // Yellow-green
    }
    else
    {
        qualityLabel = ImageQuality::GOOD;
        qualityColor = cv::Scalar(0, 255, 255); // Yellow
    }

    success_count_++;
    logger_->info("Image " + std::to_string(numCalibrationImages_ + 1) + " quality: " + imageQualityToString(qualityLabel) +
                  " (coverage:" + std::to_string(static_cast<int>(coverage * 100)) +
                  "%, sharpness:" + std::to_string(static_cast<int>(sharpness)) +
                  ", spread:" + std::to_string(static_cast<int>(normalizedSpread * 100)) + "%)");

    // Refine corner locations for sub-pixel accuracy
    cv::Size winSize = (calibrationModel_ == CalibrationModel::FISHEYE) ? cv::Size(5, 5) : cv::Size(11, 11);

    cv::cornerSubPix(
        image,
        corners,
        winSize,
        cv::Size(-1, -1),
        cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 30, 0.01)
        );
    lastImagePoint_ = corners;

    // Generate 3D object points for this chessboard
    std::vector<cv::Point3f> objp;
    for (uint32_t r = 0; r < checkerboardDimensions_[1]; ++r)
    {
        for (uint32_t c = 0; c < checkerboardDimensions_[0]; ++c)
        {
            objp.emplace_back(c, r, 0.0f);
        }
    }
    lastObjectPoint_ = objp;
    // Add quality indicator with background box for better readability
    std::string qualityText = imageQualityToString(qualityLabel) + " COVERAGE: (" + std::to_string(static_cast<int>(coverage * 100)) + "%), SHARPNESS: (" +
                              std::to_string(static_cast<int>(sharpness)) + "), SPREAD: (" + std::to_string(static_cast<int>(normalizedSpread * 100)) + "%)";

    // Calculate text size to determine background box dimensions
    int baseline = 0;
    cv::Size textSize = cv::getTextSize(qualityText, cv::FONT_HERSHEY_SIMPLEX, 1.0, 2, &baseline);

    // Draw semi-transparent black background box
    cv::Point textOrg(10, 30);
    cv::rectangle(debugImage,
                  textOrg + cv::Point(0, baseline),
                  textOrg + cv::Point(textSize.width, -textSize.height),
                  cv::Scalar(0, 0, 0),
                  cv::FILLED);

    // Draw text on top of background
    cv::putText(debugImage, qualityText, textOrg, cv::FONT_HERSHEY_SIMPLEX, 1.0, qualityColor, 2);
    // Add image index and checkerboard info
    cv::putText(debugImage, "Image " + std::to_string(numCalibrationImages_ + 1), cv::Point(10, 70),
                cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 2);
    cv::putText(debugImage, "Board: " + std::to_string(checkerboardDimensions_[0]) + "x" + std::to_string(checkerboardDimensions_[1]),
                cv::Point(10, 100), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);
    return qualityLabel;
    // Save debug image with annotations
    // std::string debugAnnotatedPath = "/tmp/debug_cal_image_" +
    // std::to_string(numCalibrationImages_ + 1) + "_processed.jpg";
    // cv::imwrite(debugAnnotatedPath, debugImage);
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

        std::vector<cv::Point2f> corners;
        bool found = false;

        if (calibrationModel_ == CalibrationModel::FISHEYE)
        {
            // Fisheye calibration: use minimal preprocessing to avoid
            // introducing artifacts
            logger_->info("Using fisheye-optimized corner detection for image " + std::to_string(i + 1));

            found = cv::findChessboardCorners(
                calibrationImages_[i],
                cv::Size(checkerboardDimensions_[0], checkerboardDimensions_[1]),
                corners,
                cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE
                );

            // If that fails, try the a slightly more aggressive approach (but
            // avoid CLAHE for fisheye as it can hurt detection)
            // if (!found)
            // {
            //     logger_->info("Fisheye detection: trying original image...");
            //     // Try with gentle blur only (no CLAHE - it can hurt fisheye
            // detection)
            //     cv::Mat blurred;
            //     cv::GaussianBlur(calibrationImages_[i], blurred, cv::Size(3,
            // 3), 0.5);

            //     found = cv::findChessboardCorners(
            //         blurred,
            //         cv::Size(checkerboardDimensions_[0],
            // checkerboardDimensions_[1]),
            //         corners,
            //         cv::CALIB_CB_ADAPTIVE_THRESH |
            // cv::CALIB_CB_NORMALIZE_IMAGE
            //     );
            // }
        }
        else
        {
            // Standard calibration: can use more aggressive preprocessing
            logger_->info("Using standard corner detection for image " + std::to_string(i + 1));

            logger_->info("Standard detection: trying original image...");
            found = cv::findChessboardCorners(
                calibrationImages_[i],
                cv::Size(checkerboardDimensions_[0], checkerboardDimensions_[1]),
                corners,
                cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE
                );

            if (!found)
            {
                logger_->info("Standard detection: trying processed image...");
                cv::Mat processedImage;
                cv::GaussianBlur(calibrationImages_[i], processedImage, cv::Size(3, 3), 0.5);
                cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
                clahe->apply(processedImage, processedImage);
                found = cv::findChessboardCorners
                        (
                    processedImage,
                    cv::Size(checkerboardDimensions_[0], checkerboardDimensions_[1]),
                    corners,
                    cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE | cv::CALIB_CB_FILTER_QUADS
                        );
            }
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
            // Quality screen 1: Coverage - Ensure corners are well-distributed
            // across the image
            cv::Rect bbox = cv::boundingRect(corners);
            double coverage = (bbox.width * bbox.height) / static_cast<double>(calibrationImages_[i].cols * calibrationImages_[i].rows);

            // Quality screen 2: Sharpness - Use Laplacian variance to detect
            // blur
            cv::Mat laplacian;
            cv::Laplacian(calibrationImages_[i](bbox), laplacian, CV_64F);
            cv::Scalar mean, stddev;
            cv::meanStdDev(laplacian, mean, stddev);
            double sharpness = stddev[0] * stddev[0]; // Variance

            // Quality screen 3: Corner spread - Ensure corners aren't clustered
            // Calculate center of mass and average distance from center
            cv::Point2f centerOfMass(0, 0);
            for (const auto &corner : corners)
            {
                centerOfMass += corner;
            }
            centerOfMass *= (1.0f / corners.size());

            double avgDistFromCenter = 0.0;
            for (const auto &corner : corners)
            {
                avgDistFromCenter += cv::norm(corner - centerOfMass);
            }
            avgDistFromCenter /= corners.size();

            // Normalize spread by image diagonal
            double imageDiagonal = std::sqrt(calibrationImages_[i].cols * calibrationImages_[i].cols +
                                             calibrationImages_[i].rows * calibrationImages_[i].rows);
            double normalizedSpread = avgDistFromCenter / imageDiagonal;

            std::string rejectReason;
            bool accept = true;

            // Coverage check: Reject if too small or too large
            if (coverage < CHECKERBOARD_COVERAGE_TOO_LOW)
            {
                rejectReason = "TOO FAR (" + std::to_string(static_cast<int>(coverage * 100)) + "%)";
                accept = false;
            }
            else if (coverage > CHECKERBOARD_COVERAGE_TOO_HIGH)
            {
                rejectReason = "TOO CLOSE (" + std::to_string(static_cast<int>(coverage * 100)) + "%)";
                accept = false;
            }
            // Sharpness check: Reject if too blurry (threshold depends on image
            // resolution)
            else if (sharpness < SHARPNESS_THRESHOLD_LOW) // Adjust threshold
                                                          // based on your
                                                          // images
            {
                rejectReason = "TOO BLURRY (sharpness: " + std::to_string(static_cast<int>(sharpness)) + ")";
                accept = false;
            }
            // Spread check: Reject if corners too clustered (poor geometry)
            else if (normalizedSpread < MIN_CORNER_CLUSTERING) // Corners should
                                                               // span at least
                                                               // 15% of
                                                               // diagonal
            {
                rejectReason = "CLUSTERED (spread: " + std::to_string(static_cast<int>(normalizedSpread * 100)) + "%)";
                accept = false;
            }

            if (!accept)
            {
                logger_->warning("Image " + std::to_string(i + 1) + ": " + rejectReason);
                fail_count_++;
                cv::putText(debugImage, rejectReason, cv::Point(10, 30),
                            cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 165, 255), 2);
            }
            else
            {
                // Image passed all quality screens - categorize quality based
                // on how well it meets criteria
                std::string qualityLabel;
                cv::Scalar qualityColor;

                if (coverage >= CHECKERBOARD_COVERAGE_GOOD && sharpness > SHARPNESS_THRESHOLD_HIGH && normalizedSpread > CORNER_CLUSTERING_EXCELLENT)
                {
                    qualityLabel = "EXCELLENT";
                    qualityColor = cv::Scalar(0, 255, 0); // Green
                }
                else if (coverage >= CHECKERBOARD_COVERAGE_TOO_LOW && sharpness > SHARPNESS_THRESHOLD_LOW && normalizedSpread > CORNER_CLUSTERING_GOOD)
                {
                    qualityLabel = "VERY GOOD";
                    qualityColor = cv::Scalar(0, 200, 200); // Yellow-green
                }
                else
                {
                    qualityLabel = "GOOD";
                    qualityColor = cv::Scalar(0, 255, 255); // Yellow
                }

                success_count_++;
                logger_->info("Image " + std::to_string(i + 1) + " quality: " + qualityLabel +
                              " (coverage:" + std::to_string(static_cast<int>(coverage * 100)) +
                              "%, sharpness:" + std::to_string(static_cast<int>(sharpness)) +
                              ", spread:" + std::to_string(static_cast<int>(normalizedSpread * 100)) + "%)");

                // Refine corner locations for sub-pixel accuracy
                cv::Size winSize = (calibrationModel_ == CalibrationModel::FISHEYE) ? cv::Size(5, 5) : cv::Size(11, 11);

                cv::cornerSubPix(
                    calibrationImages_[i],
                    corners,
                    winSize,
                    cv::Size(-1, -1),
                    cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 30, 0.01)
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

                // Add quality indicator
                cv::putText(debugImage, qualityLabel + " (" + std::to_string(static_cast<int>(coverage * 100)) + "%)",
                            cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1.0, qualityColor, 2);
            }
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
        std::string debugAnnotatedPath = "/tmp/debug_cal_image_" +
                                         std::to_string(i + 1) + "_processed.jpg";
        cv::imwrite(debugAnnotatedPath, debugImage);
    }

    logger_->info("Corner detection complete: " + std::to_string(success_count_) + " success, " +
                  std::to_string(fail_count_) + " failed");
    return true;
}

// CharucoCalibration::CharucoCalibration()
//     : CalibrateDistortion()
// {
// }

// CharucoCalibration::~CharucoCalibration()
// {
// }

// bool CharucoCalibration::findObjectAndImagePoints()
// {
//     logger_->info("Starting CharuCo calibration with " +
// std::to_string(numCalibrationImages_) + " images");
//     logger_->info("CharuCo board configuration:");
//     logger_->info("  Inner corners (setDimensions): " +
// std::to_string(checkerboardDimensions_[0]) + "x" +
// std::to_string(checkerboardDimensions_[1]));
//     logger_->info("  Board squares: " +
// std::to_string(checkerboardDimensions_[0] + 1) + "x" +
// std::to_string(checkerboardDimensions_[1] + 1));
//     logger_->info("  Square size: " + std::to_string(squareLength_) + "m");
//     logger_->info("  Marker size: " + std::to_string(markerLength_) + "m");
//     logger_->info("  Dictionary ID: " + std::to_string(arucoDictId_));

//     // Create CharuCo board configuration
//     // CharuCo board has (checkerboardDimensions + 1) squares per side
//     // ArUco markers are placed in the white squares
//     cv::Ptr<cv::aruco::Dictionary> dictionary =
// cv::makePtr<cv::aruco::Dictionary>(
//         cv::aruco::getPredefinedDictionary(arucoDictId_)
//     );
//     cv::Ptr<cv::aruco::CharucoBoard> charucoBoard =
// cv::makePtr<cv::aruco::CharucoBoard>(
//         cv::Size(checkerboardDimensions_[0] + 1, checkerboardDimensions_[1] +
// 1),  // Number of squares
//         squareLength_,  // Square side length
//         markerLength_,  // Marker side length
//         *dictionary
//     );

//     logger_->info("CharuCo board created with " +
// std::to_string(charucoBoard->getChessboardSize().width) + "x" +
//                  std::to_string(charucoBoard->getChessboardSize().height) + "
// squares, " +
//                  std::to_string(charucoBoard->getChessboardCorners().size())
// + " corners");

//     cv::Ptr<cv::aruco::DetectorParameters> detectorParams =
// cv::makePtr<cv::aruco::DetectorParameters>();

//     // Optimize detector parameters for fisheye if needed
//     if (calibrationModel_ == CalibrationModel::FISHEYE)
//     {
//         detectorParams->adaptiveThreshWinSizeMin = 3;
//         detectorParams->adaptiveThreshWinSizeMax = 23;
//         detectorParams->adaptiveThreshWinSizeStep = 10;
//         detectorParams->cornerRefinementMethod =
// cv::aruco::CORNER_REFINE_SUBPIX;
//     }
//     else
//     {
//         detectorParams->cornerRefinementMethod =
// cv::aruco::CORNER_REFINE_CONTOUR;
//     }

//     // Process each calibration image
//     for(size_t i = 0; i < numCalibrationImages_; ++i)
//     {
//         // Validate image before processing
//         if (calibrationImages_[i].empty())
//         {
//             logger_->error("Image " + std::to_string(i + 1) + " is empty,
// skipping");
//             fail_count_++;
//             continue;
//         }

//         std::vector<int> markerIds;
//         std::vector<std::vector<cv::Point2f>> markerCorners;
//         std::vector<cv::Point2f> charucoCorners;
//         std::vector<int> charucoIds;

//         // Detect ArUco markers
//         cv::aruco::detectMarkers(calibrationImages_[i], dictionary,
// markerCorners, markerIds, detectorParams);

//         logger_->info("Image " + std::to_string(i + 1) + ": Detected " +
// std::to_string(markerIds.size()) + " ArUco markers");

//         // Log first few marker IDs to verify they match the expected range
//         if (markerIds.size() > 0) {
//             std::string idList = "";
//             for (size_t j = 0; j < std::min(size_t(10), markerIds.size());
// ++j) {
//                 idList += std::to_string(markerIds[j]) + " ";
//             }
//             logger_->info("  First marker IDs: " + idList + (markerIds.size()
// > 10 ? "..." : ""));
//         }

//         // Create debug image
//         cv::Mat debugImage = calibrationImages_[i].clone();
//         if (debugImage.channels() == 1)
//         {
//             cv::cvtColor(debugImage, debugImage, cv::COLOR_GRAY2BGR);
//         }

//         bool found = false;
//         if (markerIds.size() > 0)
//         {
//             // Log marker ID range for debugging
//             int minId = *std::min_element(markerIds.begin(),
// markerIds.end());
//             int maxId = *std::max_element(markerIds.begin(),
// markerIds.end());
//             logger_->info("  Marker ID range: " + std::to_string(minId) + "
// to " + std::to_string(maxId));

//             // Interpolate CharuCo corners from detected markers
//             int numInterpolated =
// cv::aruco::interpolateCornersCharuco(markerCorners, markerIds,
// calibrationImages_[i],
//                                                  charucoBoard,
// charucoCorners, charucoIds);

//             logger_->info("Image " + std::to_string(i + 1) + ": Interpolated
// " + std::to_string(charucoCorners.size()) + " CharuCo corners (return value:
// " + std::to_string(numInterpolated) + ")");

//             // We need at least 4 corners for calibration
//             if (charucoCorners.size() >= 4)
//             {
//                 found = true;
//                 success_count_++;

//                 // Store the detected corners and their corresponding 3D
// object points
//                 imagePoints_.push_back(charucoCorners);

//                 // Get the 3D object points for the detected CharuCo corners
//                 std::vector<cv::Point3f> objPoints;
//                 std::vector<cv::Point3f> boardCorners =
// charucoBoard->getChessboardCorners();
//                 for (size_t j = 0; j < charucoIds.size(); ++j)
//                 {
//                     cv::Point3f objPoint = boardCorners[charucoIds[j]];
//                     objPoints.push_back(objPoint);
//                 }
//                 objectPoints_.push_back(objPoints);

//                 // Draw detected markers and CharuCo corners
//                 cv::aruco::drawDetectedMarkers(debugImage, markerCorners,
// markerIds);
//                 cv::aruco::drawDetectedCornersCharuco(debugImage,
// charucoCorners, charucoIds, cv::Scalar(0, 255, 0));

//                 cv::putText(debugImage, "FOUND: " +
// std::to_string(charucoCorners.size()) + " corners",
//                            cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1.0,
// cv::Scalar(0, 255, 0), 2);
//             }
//             else
//             {
//                 fail_count_++;
//                 cv::aruco::drawDetectedMarkers(debugImage, markerCorners,
// markerIds);
//                 cv::putText(debugImage, "INSUFFICIENT CORNERS: " +
// std::to_string(charucoCorners.size()),
//                            cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1.0,
// cv::Scalar(0, 165, 255), 2);
//                 logger_->warning("Image " + std::to_string(i + 1) + ":
// Insufficient CharuCo corners (" +
//                                std::to_string(charucoCorners.size()) + " <
// 4)");
//             }
//         }
//         else
//         {
//             fail_count_++;
//             cv::putText(debugImage, "NO MARKERS DETECTED", cv::Point(10, 30),
//                        cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 255),
// 2);
//             logger_->warning("Image " + std::to_string(i + 1) + ": No ArUco
// markers detected");
//         }

//         // Add image index and board info
//         cv::putText(debugImage, "Image " + std::to_string(i + 1),
// cv::Point(10, 70),
//                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255),
// 2);
//         cv::putText(debugImage, "CharuCo Board: " +
// std::to_string(checkerboardDimensions_[0] + 1) + "x" +
//                    std::to_string(checkerboardDimensions_[1] + 1),
// cv::Point(10, 100),
//                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255),
// 1);

//         drawnCalibrationImages_.push_back(debugImage);

//         // Save debug image with annotations
//         std::string debugAnnotatedPath = "/tmp/debug_charuco_image_" +
// std::to_string(i + 1) + "_processed.jpg";
//         cv::imwrite(debugAnnotatedPath, debugImage);
//     }

//     logger_->info("CharuCo detection complete: " +
// std::to_string(success_count_) + " success, " +
//                  std::to_string(fail_count_) + " failed");
//     return true;
// }
} // End namespace PiTrac